/* Preset programmes: published base rules as of October 2026. Every value
 * is editable on the device; these are starting points. Money-valued
 * programmes count in cents (valuePerPoint 1000 thousandths of a cent). */
#include "presets.h"
#include "currency.h"

/* Section placement: see sections.h. */
static void Cp(char *d, const char *s, UInt16 n) EXPORT_SEC;

static void Cp(char *d, const char *s, UInt16 n)
{
    UInt16 i = 0;
    while (s[i] && i + 1 < n) { d[i] = s[i]; i++; }
    d[i] = 0;
}

void PgmPreset(UInt8 i, ProgrammeRecord *r)
{
    UInt8 *p = (UInt8 *)r;
    UInt32 k;
    for (k = 0; k < sizeof *r; k++) p[k] = 0;
    r->id = (UInt8)(i + 1);
    r->premiumMult = 100;
    switch (i) {
    case 0:  /* 76: 3 c/US gal on the card, Card fills only, balance in dollars */
        Cp(r->name, "76 / Phillips 66", pgmNameLen);
        r->currency = curUSD; r->earnBasis = pgmEarnPerUSGallon; r->earnRate = 300;
        r->requiresPayment = pgmPaymentCard; r->autoApply = 1; r->valuePerPoint = 1000;
        r->redeemKind = pgmRedeemMoneyOff; r->redeemBlock = 1; r->redeemValue = 10;
        Cp(r->aliases, "KB Points,KB,Conoco", pgmAliasLen);
        break;
    case 1:  /* Petro-Points: 10 pts/L, 1,000 pts = $1 off */
        Cp(r->name, "Petro-Points", pgmNameLen);
        r->currency = curCAD; r->earnBasis = pgmEarnPerLitre; r->earnRate = 1000;
        r->autoApply = 1; r->valuePerPoint = 100;
        r->redeemKind = pgmRedeemMoneyOff; r->redeemBlock = 1000; r->redeemValue = 1000;
        Cp(r->aliases, "PP,Petro", pgmAliasLen);
        break;
    case 2:  /* Triangle Rewards: 3 c/L in CT Money at Petro-Canada */
        Cp(r->name, "Triangle Rewards", pgmNameLen);
        r->currency = curCAD; r->earnBasis = pgmEarnPerLitre; r->earnRate = 300;
        r->autoApply = 1; r->valuePerPoint = 1000;
        r->redeemKind = pgmRedeemMoneyOff; r->redeemBlock = 1; r->redeemValue = 10;
        Cp(r->aliases, "CT Money,Triangle", pgmAliasLen);
        break;
    case 3:  /* PC Optimum: 10 pts/L at Esso; 4,000 pts = 10 c/L off up to 40 L */
        Cp(r->name, "PC Optimum", pgmNameLen);
        r->currency = curCAD; r->earnBasis = pgmEarnPerLitre; r->earnRate = 1000;
        r->redeemKind = pgmRedeemPerVolume; r->redeemBlock = 4000; r->redeemValue = 100;
        r->redeemVolUnit = pgmVolLitre; r->redeemMaxVol = 40000;
        Cp(r->aliases, "PCO,Esso", pgmAliasLen);
        break;
    case 4:  /* Kroger: earned on groceries; 100 pts = 10 c/gal off, to $1/gal on 35 gal */
        Cp(r->name, "Kroger Fuel Points", pgmNameLen);
        r->currency = curUSD; r->earnBasis = pgmEarnNone;
        r->redeemKind = pgmRedeemPerVolume; r->redeemBlock = 100; r->redeemValue = 100;
        r->redeemVolUnit = pgmVolUSGallon; r->redeemMaxVol = 132489; r->redeemMaxPerUnit = 1000;
        r->expiryRule = pgmExpiryMonthAfter;
        Cp(r->aliases, "Fred Meyer,King Soopers", pgmAliasLen);
        break;
    default: /* Credit card points: rate left blank, Card fills only */
        Cp(r->name, "Credit card points", pgmNameLen);
        r->currency = curCAD; r->earnBasis = pgmEarnNone; r->requiresPayment = pgmPaymentCard;
        break;
    }
}
