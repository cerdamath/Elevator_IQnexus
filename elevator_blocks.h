/**
 * Elevator Control System - 150 Single Frame Blocks
 * ==================================================
 * 
 * Each block is a single 36-char frame
 * Frame structure: [H + car + dir + lvl + space + ocss + space + mcss + space + doors + \n + inputs + \x1B
 * 
 * Generated with random variations of:
 *   - Car IDs: A, B, C
 *   - Directions: '-' (down), 'u' (up), 'd' (door/idle)
 *   - Levels: 00-31
 *   - OCSS states: 43 different codes
 *   - MCSS states: 12 different codes
 *   - Door states: [], ][, <>, ><
 *   - System inputs: 120+ different codes
 */

#ifndef ELEVATOR_BLOCKS_H
#define ELEVATOR_BLOCKS_H

char block1[] = "[HB-04 WCO EW []<>\n^SGS CTO UIB L30\x1B";
char block2[] = "[HBu16 CTL CR <><>\nFAN ^DCL EDB TDC\x1B";
char block3[] = "[HBd12 EHS SR <><>\nGSM NUG DHB ^MDD\x1B";
char block4[] = "[HAu10 NOR ES []><\nUIB EDP ^GSM LWO\x1B";
char block5[] = "[HA-13 DTO FR ][[]\nDCL LRD EQW ^WDO\x1B";
char block6[] = "[HCd02 INI SR ][[]\nWDO ^WDC HCO ISS\x1B";
char block7[] = "[HBu03 EPW RS <>[]\nEQS TCI ^DOS ESH\x1B";
char block8[] = "[HC-02 INS EW ][><\nPKG ^TDO ETS HCH\x1B";
char block9[] = "[HBd25 ARD RL <>][\nERO WDC ^WDC FAN\x1B";
char block10[] = "[HB-19 HBP SR ][><\n2EF ^TDO LRD TCB\x1B";
char block11[] = "[HB-17 EPR EF <>[]\n^DOL ESK EQW ETS\x1B";
char block12[] = "[HC-25 COR FR []][\nCCT AEF CCT ^SDB\x1B";
char block13[] = "[HBu12 EFO IN <>[]\n2LV LNS ^MDD EQ1\x1B";
char block14[] = "[HCd06 ESB SR <>][\nWDO ^TDC DES 1LV\x1B";
char block15[] = "[HAu17 CBP RL ][<>\nRTB ^MDD DDS LNS\x1B";
char block16[] = "[HA-24 HBP NR []][\nRRB ^DOL PKS DCB\x1B";
char block17[] = "[HB-12 CHC SR ][<>\n^WDO L30 L50 ASL\x1B";
char block18[] = "[HB-05 WCO SR [][]\nGCB LNS ^TDO 2LS\x1B";
char block19[] = "[HAd03 NOR ES ><][\nCCB L50 NUG ^TDO\x1B";
char block20[] = "[HB-07 ATT EF <><>\nEQW ^WDO EQ1 EQR\x1B";
char block21[] = "[HA-05 ANS ST ><[]\nDDS ^SGS 2SE CTO\x1B";
char block22[] = "[HBu24 LNS ES []><\n^SDB HAD 2TH NAV\x1B";
char block23[] = "[HAd28 EHS EF <><>\n^GSM ISP CTL EFO\x1B";
char block24[] = "[HAd20 NAV CR ][><\n^DOS LNS HFA PKS\x1B";
char block25[] = "[HAu23 INS EF ><][\nRRB L30 ^SDB HFA\x1B";
char block26[] = "[HAu09 COR CR <>][\nCCT ACH ESH ^TDC\x1B";
char block27[] = "[HAu04 DCS FR ><><\n^DCL LNS TCB XEF\x1B";
char block28[] = "[HA-25 REI SR ><][\nRRB ^DOB NUG ISS\x1B";
char block29[] = "[HC-08 EFS ID ><][\n^GCB CRC 1EF ADB\x1B";
char block30[] = "[HAu15 EPW FR [][]\nCOC WDO ^WDC EQ1\x1B";
char block31[] = "[HAu22 DLM SR []><\nEQ2 NOR ^DOB GDS\x1B";
char block32[] = "[HAu06 EFO EF <>[]\n^TDC 2LV 2TH DDS\x1B";
char block33[] = "[HAd19 DHB FR <>][\n^DOB COC ACH ISP\x1B";
char block34[] = "[HAu26 EMT CR [][]\nASL PKS NUD ^LRD\x1B";
char block35[] = "[HBd21 WCS EF <>[]\n1LS L30 DFC ^DOS\x1B";
char block36[] = "[HCu28 ARD SR ][[]\n1TH 1LV PKG ^LRD\x1B";
char block37[] = "[HAd17 OLD IN ][<>\n^WDC CFS EDP 2TH\x1B";
char block38[] = "[HB-06 ISC EW <>[]\n2EF DHB PKG ^SDB\x1B";
char block39[] = "[HC-21 DLM RS []<>\n^TDO EFO ASL DCB\x1B";
char block40[] = "[HAd04 ATT CR [][]\n^ISS NAV ISS CHC\x1B";
char block41[] = "[HCu16 ACP SR ><[]\nLNS WDC EDP ^SDB\x1B";
char block42[] = "[HC-24 DLM ST ][][\n^ISS GSM ACC 2LV\x1B";
char block43[] = "[HB-25 INI RL ><<>\nEFB EFO ^DOS HAD\x1B";
char block44[] = "[HAd13 DLM ID []<>\nCOC ^GSM PKG DDS\x1B";
char block45[] = "[HC-30 ISC CR ][<>\nCCT ESK HFA ^TDC\x1B";
char block46[] = "[HCd23 OLD NR []][\nACH EDB NAV ^WDC\x1B";
char block47[] = "[HAd28 IDL NR <>[]\nNOR PDD ^DCL DES\x1B";
char block48[] = "[HA-31 DTO EW <>][\nWDC ^TDC CCB PKG\x1B";
char block49[] = "[HC-04 EFO EF <><>\nROT NUD ^ISS EFO\x1B";
char block50[] = "[HCu20 LNS ES []][\nDCL TDO CFS ^DOS\x1B";
char block51[] = "[HBd29 MIT ID ][<>\nXEF ^SDB EFB CTC\x1B";
char block52[] = "[HAd11 ACP FR ][][\nESK 2TH ^WDC CCT\x1B";
char block53[] = "[HC-06 IDL ES ][><\nLNS ^TDC SDB NOR\x1B";
char block54[] = "[HB-11 EQR ST <>><\nBRK ^FAN EQW COH\x1B";
char block55[] = "[HAu27 EFS SR <>][\nEFO ERO 1LV ^WDC\x1B";
char block56[] = "[HBd21 INI ID ][][\nEQ2 ^SDB CHC DCB\x1B";
char block57[] = "[HC-00 REI EF <>[]\nSGS DCB 2TH ^FAN\x1B";
char block58[] = "[HA-22 EPR ID [][]\n^GSM PKS FAN CFS\x1B";
char block59[] = "[HAu27 REI SR ][][\nEDB LWO DFD ^DOL\x1B";
char block60[] = "[HBd08 WCO IN []][\n^LRD WDC HTS RRB\x1B";
char block61[] = "[HBd29 HAD ES ][[]\nSDB ^DHB ACC ESK\x1B";
char block62[] = "[HC-10 DBF IN []<>\nIST LNS CTL ^WDC\x1B";
char block63[] = "[HCu01 GCB FR []><\n^DCB BRK NOR ROT\x1B";
char block64[] = "[HAu23 OLD CR ][><\n2TH NUD CCT ^EDP\x1B";
char block65[] = "[HAu18 EPW EW ><><\nCTC HFA MDD ^ISS\x1B";
char block66[] = "[HCd02 ATT SR <><>\nLNS LNS GCO ^ISS\x1B";
char block67[] = "[HBd12 PRK FR []><\nDRD ^GSM FAN CCB\x1B";
char block68[] = "[HAu26 ACP CR ][<>\n^WDO ETS 1TH SDB\x1B";
char block69[] = "[HAd19 DLM NR <><>\n^TDO ISP LWX FAN\x1B";
char block70[] = "[HBu06 ROT NR []><\nTCI EQW ^ISS LRD\x1B";
char block71[] = "[HC-08 HAD NR <>><\n^DOB ERO DHB PKS\x1B";
char block72[] = "[HB-31 NAV FR <>><\n^TDC GSI GDS 1TH\x1B";
char block73[] = "[HA-18 ROT RS ][<>\nNRF ASL ISS ^FAN\x1B";
char block74[] = "[HBu14 PKS CR ><<>\nHFA ^DOB GCB HAD\x1B";
char block75[] = "[HBd02 INS ID [][]\nLWO ^WDO EDP COC\x1B";
char block76[] = "[HCu17 DCP RL []<>\n^DOB EQ2 CHC DFC\x1B";
char block77[] = "[HCd03 ISC EW []][\nCFB TCI 2TH ^DCB\x1B";
char block78[] = "[HBd04 WCS ST []><\nWDC ^GCB GSI L30\x1B";
char block79[] = "[HAd22 ACP RS <>><\nCCB EFO ACC ^GCB\x1B";
char block80[] = "[HA-31 EFS SR []><\nBRK BRK ^LRD RTB\x1B";
char block81[] = "[HCd24 CHC FR <>><\nPKS XEF TDC ^DOS\x1B";
char block82[] = "[HAd14 LNS FR ><><\nAEF ISS COH ^TDO\x1B";
char block83[] = "[HCu21 EQO FR <>][\nEQR ^DOS CHC 2TH\x1B";
char block84[] = "[HC-12 DHB RS ][][\nRTB DFD CRC ^WDO\x1B";
char block85[] = "[HAu08 COR EW ][[]\nLWO ACH ^LRD ESK\x1B";
char block86[] = "[HBu18 ARD FR []><\nFAN ^TDO 2LV PDD\x1B";
char block87[] = "[HA-05 HAD FR <>[]\nMDD ^EDP GCB NAV\x1B";
char block88[] = "[HAu14 EQO EF [][]\nEQ2 ^TDC FAN LWX\x1B";
char block89[] = "[HB-22 PRK EW <>[]\nEQS EFK L50 ^LRD\x1B";
char block90[] = "[HAd09 CTL RS []<>\n^GCB XEF L30 NUG\x1B";
char block91[] = "[HC-05 EFS ST <>><\nLWX ESH CCT ^SDB\x1B";
char block92[] = "[HB-10 EPR EF []><\nDIB DDS XEF ^ISS\x1B";
char block93[] = "[HCu04 EFS EW ><<>\nNUD L30 1EF ^WDC\x1B";
char block94[] = "[HAd17 INS RS <>[]\nEQS ^TDC GSM XEF\x1B";
char block95[] = "[HAd04 EFO CR []><\nEQ2 ^DOB FAN LRD\x1B";
char block96[] = "[HAu16 ESB RS ><><\n^SDB WDO BRK CTC\x1B";
char block97[] = "[HB-19 LNS ST []<>\nEFK ^TDC DDS HFA\x1B";
char block98[] = "[HBd18 CHC RS ><[]\nNUG ^EDP TDC HCO\x1B";
char block99[] = "[HB-05 ARD CR <>><\n2LV ^TDC ISP DDS\x1B";
char block100[] = "[HCd11 DCP RL ><><\nXEF ETS NUD ^DCB\x1B";
char block101[] = "[HAu26 DBF ID <><>\n^DOL ROT DES ESK\x1B";
char block102[] = "[HBu06 NAV IN [][]\n^MDD DFD PKS CRC\x1B";
char block103[] = "[HC-11 HAD RS <>><\n^DHB SDB 1LS 2TH\x1B";
char block104[] = "[HC-18 DTC EW []][\nNUG LWX 2SE ^DOS\x1B";
char block105[] = "[HCd27 ISC ST []][\nSGS ^MDD UIB L50\x1B";
char block106[] = "[HCu07 ESB EF ><][\nL30 GCB FAN ^DCL\x1B";
char block107[] = "[HCu01 ESB IN []><\nCFB HFA ETS ^GCB\x1B";
char block108[] = "[HBd08 PRK EW ><][\nCFB ^DCL COC FAN\x1B";
char block109[] = "[HA-04 ROT SR [][]\nETS BRK EQW ^TDO\x1B";
char block110[] = "[HAd21 DHB RL ][][\nNOR CTL ^WDO DES\x1B";
char block111[] = "[HAu30 LNS RS <>><\nCCT ^LRD EQS EDB\x1B";
char block112[] = "[HAd11 EMT ST ][><\n2EF ^TDO EQW 2LV\x1B";
char block113[] = "[HC-25 DTC NR ][<>\nEFO PKS ^WDO 2TH\x1B";
char block114[] = "[HC-17 EHS ST ][[]\nHCH ^SDB ISS HAD\x1B";
char block115[] = "[HA-05 DHB RL ][[]\nLNS ACC ISP ^DCL\x1B";
char block116[] = "[HCu04 INS ID <>><\nDRD ^WDC ISS GDS\x1B";
char block117[] = "[HA-18 NAV EW ][][\nADB DHB ISS ^DOL\x1B";
char block118[] = "[HC-03 EPR NR ><[]\nETS ^SDB PDD PKS\x1B";
char block119[] = "[HB-04 ATT SR []<>\n^DCL EQ2 TCB DIB\x1B";
char block120[] = "[HA-04 COR CR <>[]\n^WDO EDB IST EQR\x1B";
char block121[] = "[HCd16 COR IN <><>\nGCB ASL SDB ^ISS\x1B";
char block122[] = "[HB-15 HBP ST ][><\nEDP GDS TDO ^TDC\x1B";
char block123[] = "[HCu18 EPR EF ][[]\n^MDD SDB HTS TDO\x1B";
char block124[] = "[HCu10 EMT ST ][[]\nLWX ^DCB 2EF GSI\x1B";
char block125[] = "[HA-17 ISC RS []><\nLWX EDP NAV ^SDB\x1B";
char block126[] = "[HCd26 ARD ST ><<>\nACC SDB ^LRD CTO\x1B";
char block127[] = "[HAd12 ARD SR [][]\nDHB GCO ^FAN ETS\x1B";
char block128[] = "[HAd00 DBF ID ][][\nEFB ETS ^TDO GDS\x1B";
char block129[] = "[HAu08 CHC FR <>][\nWDC ^EDP DRD DIB\x1B";
char block130[] = "[HA-31 DTC EW <>><\n^SDB PKS 1LV LRD\x1B";
char block131[] = "[HAd27 NAV CR ><<>\n^TDO XEF 2LS LRD\x1B";
char block132[] = "[HAd14 DTO IN <><>\nL30 ^DHB NAV HAD\x1B";
char block133[] = "[HA-17 EFS IN ][][\n2LS ADB ISP ^SDB\x1B";
char block134[] = "[HBu19 EHS SR []<>\nSGS HAD ^FAN LWX\x1B";
char block135[] = "[HB-20 ARD SR [][]\nDCB SDB COC ^DOS\x1B";
char block136[] = "[HA-08 HBP IN <>][\nDDS HTS ^SDB EQS\x1B";
char block137[] = "[HAd02 WCO EW ><][\n1LS LRD ^GSM RRB\x1B";
char block138[] = "[HBd28 DCP IN <>><\nDHB FAN ^DCL EDP\x1B";
char block139[] = "[HAd11 EQR ID []<>\nHCH LNS CTL ^DCB\x1B";
char block140[] = "[HB-17 EPC FR ][<>\nHTS ^TDO TCI DHB\x1B";
char block141[] = "[HC-04 EMT NR ][][\nPDD CCB 2SE ^DOL\x1B";
char block142[] = "[HAu15 CBP FR ][<>\nHTS DIB ISS ^DCL\x1B";
char block143[] = "[HAd16 EFS NR <>><\nGSI DCL MDD ^FAN\x1B";
char block144[] = "[HAu17 ISC SR [][]\nLWO DCL ^SDB 2LV\x1B";
char block145[] = "[HA-13 EFO IN <>][\nCTC MDD CFS ^MDD\x1B";
char block146[] = "[HA-19 NOR ST <>[]\n^EDP 1LS GCB EQW\x1B";
char block147[] = "[HAd27 EPC IN ><><\nROT 2EF GDS ^SGS\x1B";
char block148[] = "[HC-15 WCS CR <>[]\nGDS DDS ^DCL ACC\x1B";
char block149[] = "[HAu12 ANS CR ][[]\nXEF HTS UIB ^DOS\x1B";
char block150[] = "[HAu06 WCS ES <>][\n^FAN NUG DCB RRB\x1B";

// Pointer table to all blocks
char *blocks[] = {
    block1, block2, block3, block4, block5, block6, block7, block8, 
    block9, block10, block11, block12, block13, block14, block15, block16, 
    block17, block18, block19, block20, block21, block22, block23, block24, 
    block25, block26, block27, block28, block29, block30, block31, block32, 
    block33, block34, block35, block36, block37, block38, block39, block40, 
    block41, block42, block43, block44, block45, block46, block47, block48, 
    block49, block50, block51, block52, block53, block54, block55, block56, 
    block57, block58, block59, block60, block61, block62, block63, block64, 
    block65, block66, block67, block68, block69, block70, block71, block72, 
    block73, block74, block75, block76, block77, block78, block79, block80, 
    block81, block82, block83, block84, block85, block86, block87, block88, 
    block89, block90, block91, block92, block93, block94, block95, block96, 
    block97, block98, block99, block100, block101, block102, block103, block104, 
    block105, block106, block107, block108, block109, block110, block111, block112, 
    block113, block114, block115, block116, block117, block118, block119, block120, 
    block121, block122, block123, block124, block125, block126, block127, block128, 
    block129, block130, block131, block132, block133, block134, block135, block136, 
    block137, block138, block139, block140, block141, block142, block143, block144, 
    block145, block146, block147, block148, block149, block150
};

#define BLOCKS_COUNT 150
#define FRAME_SIZE 36

#endif // ELEVATOR_BLOCKS_H
