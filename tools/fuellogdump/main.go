// fuellogdump reads a FuelLog backup set (the five .pdb files) on the Mac and
// prints its contents as CSV, independently of the Palm. It is the check that
// backups are readable outside the device.
//
//	fuellogdump dump <dir>        fills as CSV (the default)
//	fuellogdump vehicles <dir>
//	fuellogdump programmes <dir>
//	fuellogdump services <dir>    visits
package main

import (
	"encoding/binary"
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"time"
)

type pdb struct {
	name    string
	records [][]byte
	appInfo []byte
}

func readPdb(path string) (*pdb, error) {
	b, err := os.ReadFile(path)
	if err != nil {
		return nil, err
	}
	if len(b) < 78 {
		return nil, fmt.Errorf("%s: too short for a PDB", path)
	}
	p := &pdb{name: strings.TrimRight(string(b[:32]), "\x00")}
	appInfo := binary.BigEndian.Uint32(b[52:56])
	n := int(binary.BigEndian.Uint16(b[76:78]))
	offs := make([]uint32, n)
	for i := 0; i < n; i++ {
		e := 78 + 8*i
		if e+8 > len(b) {
			return nil, fmt.Errorf("%s: record list truncated", path)
		}
		offs[i] = binary.BigEndian.Uint32(b[e : e+4])
	}
	if appInfo != 0 && int(appInfo)+4 <= len(b) {
		p.appInfo = b[appInfo : appInfo+4]
	}
	for i := 0; i < n; i++ {
		end := uint32(len(b))
		if i+1 < n {
			end = offs[i+1]
		}
		if offs[i] > end || end > uint32(len(b)) {
			return nil, fmt.Errorf("%s: record %d out of range", path, i)
		}
		p.records = append(p.records, b[offs[i]:end])
	}
	return p, nil
}

func (p *pdb) schema() int {
	if len(p.appInfo) >= 2 {
		return int(binary.BigEndian.Uint16(p.appInfo))
	}
	return 0
}

// Palm days since 1904-01-01.
var epoch = time.Date(1904, 1, 1, 0, 0, 0, 0, time.UTC)

func dateText(days uint16) string { return epoch.AddDate(0, 0, int(days)).Format("2006-01-02") }

// Palm Latin (Windows-1252) to UTF-8.
var cp1252 = [32]rune{0x20AC, 0, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0, 0x017D, 0,
	0, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0, 0x017E, 0x0178}

func palmText(b []byte) string {
	var sb strings.Builder
	for _, c := range b {
		if c == 0 {
			break
		}
		switch {
		case c < 0x80:
			sb.WriteByte(c)
		case c < 0xA0:
			r := cp1252[c-0x80]
			if r == 0 {
				r = '?'
			}
			sb.WriteRune(r)
		default:
			sb.WriteRune(rune(c))
		}
	}
	return sb.String()
}

func cstr(b []byte, n int) string {
	if len(b) < n {
		n = len(b)
	}
	return palmText(b[:n])
}

func csvq(s string) string { return `"` + strings.ReplaceAll(s, `"`, `""`) + `"` }
func fixed(v int64, dec int) string {
	neg := v < 0
	if neg {
		v = -v
	}
	s := fmt.Sprintf("%0*d", dec+1, v)
	if dec > 0 {
		s = s[:len(s)-dec] + "." + s[len(s)-dec:]
	}
	if neg {
		s = "-" + s
	}
	return s
}

type vehicle struct {
	id         byte
	name       string
	powertrain byte
}

func vehicles(dir string) (map[byte]vehicle, []vehicle) {
	m := map[byte]vehicle{}
	var list []vehicle
	p, err := readPdb(filepath.Join(dir, "FuelVeh-ZFlg.pdb"))
	if err != nil {
		return m, list
	}
	for _, r := range p.records {
		if len(r) < 47 {
			continue
		}
		v := vehicle{id: r[0], name: cstr(r[1:25], 24), powertrain: r[25]}
		m[v.id] = v
		list = append(list, v)
	}
	return m, list
}

func programmes(dir string) (map[byte]string, [][]byte) {
	m := map[byte]string{}
	p, err := readPdb(filepath.Join(dir, "FuelPgm-ZFlg.pdb"))
	if err != nil {
		return m, nil
	}
	for _, r := range p.records {
		if len(r) < 64 {
			continue
		}
		m[r[0]] = cstr(r[1:25], 24)
	}
	return m, p.records
}

var grades = []string{"Regular", "Mid", "Premium", "Diesel", "Home AC", "Public AC", "DC fast"}
var currencies = []string{"USD", "CAD", "EUR", "AUD", "NZD"}

func name(list []string, i byte) string {
	if int(i) < len(list) {
		return list[i]
	}
	return fmt.Sprint(i)
}

func dumpFills(dir string) error {
	p, err := readPdb(filepath.Join(dir, "FuelLog-ZFlg.pdb"))
	if err != nil {
		return err
	}
	if p.schema() > 1 {
		return fmt.Errorf("schema %d is newer than this tool understands", p.schema())
	}
	veh, _ := vehicles(dir)
	pgm, _ := programmes(dir)
	fmt.Println("Date,Time,Vehicle,Odometer (km),Trip (km),Volume (L or kWh),Price (per L or kWh),Total,Currency,Rate,Payment,Grade,Full,Missed,Discount,Points,Notes")
	for _, r := range p.records {
		if len(r) < 39 {
			continue
		}
		be := binary.BigEndian
		date, tm, flags := be.Uint16(r[0:]), be.Uint16(r[2:]), be.Uint16(r[4:])
		vid, grade := r[6], r[7]
		odo, trip, vol, price, total, disc := be.Uint32(r[8:]), be.Uint32(r[12:]), be.Uint32(r[16:]), be.Uint32(r[20:]), be.Uint32(r[24:]), be.Uint32(r[28:])
		cur := r[32]
		rate := be.Uint32(r[33:])
		pay, np := r[37], int(r[38])
		pos := 39
		var pts []string
		for i := 0; i < np && pos+9 <= len(r); i++ {
			id := r[pos]
			earned := int32(be.Uint32(r[pos+1:]))
			redeemed := int32(be.Uint32(r[pos+5:]))
			n, ok := pgm[id]
			if !ok {
				n = fmt.Sprintf("programme %d", id)
			}
			s := fmt.Sprintf("%s +%d", n, earned)
			if redeemed != 0 {
				s += fmt.Sprintf(" -%d", redeemed)
			}
			pts = append(pts, s)
			pos += 9
		}
		notes := ""
		if pos < len(r) {
			notes = palmText(r[pos:])
		}
		timeText := ""
		if tm != 0xFFFF {
			timeText = fmt.Sprintf("%02d:%02d", tm/60, tm%60)
		}
		charge := flags&8 != 0
		volText := fixed(int64(vol), 3)
		if !charge {
			volText = fixed(int64(vol), 3)
		}
		fullText, missedText := "", ""
		if flags&1 != 0 {
			fullText = "yes"
		}
		if flags&2 != 0 {
			missedText = "yes"
		}
		payText := "Cash"
		if pay == 1 {
			payText = "Card"
		} else if charge {
			payText = "Bill"
		}
		tripText := ""
		if trip != 0 {
			tripText = fixed(int64(trip), 1)
		}
		discText := ""
		if disc != 0 {
			discText = fixed(int64(disc), 2)
		}
		fmt.Printf("%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n",
			dateText(date), timeText, csvq(veh[vid].name), fixed(int64(odo), 1), tripText, volText, fixed(int64(price), 4),
			fixed(int64(total), 2), name(currencies, cur), fixed(int64(rate/100), 4), payText, name(grades, grade), fullText, missedText, discText,
			csvq(strings.Join(pts, "; ")), csvq(notes))
	}
	return nil
}

func dumpVehicles(dir string) error {
	_, list := vehicles(dir)
	power := []string{"Fuel", "Hybrid", "Plug-in hybrid", "Electric"}
	fmt.Println("Id,Vehicle,Powertrain")
	for _, v := range list {
		fmt.Printf("%d,%s,%s\n", v.id, csvq(v.name), name(power, v.powertrain))
	}
	return nil
}

func dumpProgrammes(dir string) error {
	_, recs := programmes(dir)
	fmt.Println("Id,Programme,Currency,Earn rate x100,Basis,Opening,Aliases")
	for _, r := range recs {
		be := binary.BigEndian
		fmt.Printf("%d,%s,%s,%d,%d,%d,%s\n", r[0], csvq(cstr(r[1:25], 24)), name(currencies, r[25]), be.Uint32(r[27:]), r[26], int32(be.Uint32(r[34:])), csvq(palmText(r[64:])))
	}
	return nil
}

func dumpServices(dir string) error {
	p, err := readPdb(filepath.Join(dir, "FuelSvL-ZFlg.pdb"))
	if err != nil {
		return err
	}
	veh, _ := vehicles(dir)
	fmt.Println("Date,Vehicle,Odometer (km),Services mask,Cost,Currency,Payment,Shop,Detail,Notes")
	for _, r := range p.records {
		if len(r) < 101 {
			continue
		}
		be := binary.BigEndian
		fmt.Printf("%s,%s,%s,%08x,%s,%s,%s,%s,%s,%s\n", dateText(be.Uint16(r[0:])), csvq(veh[r[2]].name), fixed(int64(be.Uint32(r[3:])), 1), be.Uint32(r[7:]),
			fixed(int64(be.Uint32(r[35:])), 2), name(currencies, r[39]), map[byte]string{0: "Cash", 1: "Card"}[r[44]], csvq(cstr(r[45:69], 24)), csvq(cstr(r[69:101], 32)), csvq(palmText(r[101:])))
	}
	return nil
}

func main() {
	cmd, dir := "dump", "."
	if len(os.Args) > 1 {
		cmd = os.Args[1]
	}
	if len(os.Args) > 2 {
		dir = os.Args[2]
	}
	var err error
	switch cmd {
	case "dump", "fills":
		err = dumpFills(dir)
	case "vehicles":
		err = dumpVehicles(dir)
	case "programmes":
		err = dumpProgrammes(dir)
	case "services":
		err = dumpServices(dir)
	default:
		err = fmt.Errorf("unknown command %q", cmd)
	}
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
}
