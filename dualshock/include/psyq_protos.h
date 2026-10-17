/*
 * psyq_protos.h - prototypes of the 188 Sony PsyQ functions RE1's Capcom code calls
 * (dualshock/tools/port_inventory.py --list), written from the documented library interface.
 * Used as m2c context and for new drafts; integrate.py copies the prototypes a function uses
 * into its body (old functions matched with m2c's guessed block-scope declarations, which
 * would conflict with file-scope prototypes).
 */
#ifndef PSYQ_PROTOS_H
#define PSYQ_PROTOS_H

#include "psyq_types.h"

/* ---------------------------------------------------------------- libgte */
MATRIX *ApplyMatrix(MATRIX *m, SVECTOR *v0, VECTOR *v1);
VECTOR *ApplyMatrixLV(MATRIX *m, VECTOR *v0, VECTOR *v1);
SVECTOR *ApplyMatrixSV(MATRIX *m, SVECTOR *v0, SVECTOR *v1);
MATRIX *CompMatrix(MATRIX *m0, MATRIX *m1, MATRIX *m2);
MATRIX *MulMatrix(MATRIX *m0, MATRIX *m1);
MATRIX *MulMatrix0(MATRIX *m0, MATRIX *m1, MATRIX *m2);
MATRIX *MulMatrix2(MATRIX *m0, MATRIX *m1);
MATRIX *RotMatrix(SVECTOR *r, MATRIX *m);
MATRIX *RotMatrixY(long r, MATRIX *m);
MATRIX *RotMatrixYXZ(SVECTOR *r, MATRIX *m);
MATRIX *ScaleMatrix(MATRIX *m, VECTOR *v);
MATRIX *TransposeMatrix(MATRIX *m0, MATRIX *m1);
void SetRotMatrix(MATRIX *m);
void SetTransMatrix(MATRIX *m);
void SetLightMatrix(MATRIX *m);
void SetGeomOffset(long ofx, long ofy);
long RotTransPers(SVECTOR *v0, long *sxy, long *p, long *flag);
long RotAverage3(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, long *sxy0, long *sxy1, long *sxy2,
                 long *p, long *flag);
long RotAverage4(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, SVECTOR *v3, long *sxy0, long *sxy1,
                 long *sxy2, long *sxy3, long *p, long *flag);
void OuterProduct0(VECTOR *v0, VECTOR *v1, VECTOR *v2);
void VectorNormal(VECTOR *v0, VECTOR *v1);
long SquareRoot0(long a);
long SquareRoot12(long a);
long LoadAverageShort12(SVECTOR *v0, SVECTOR *v1, long p0, long p1, SVECTOR *v2);
long ratan2(long y, long x);
long catan(long a);
int rsin(int a);
/* ---------------------------------------------------------------- libgpu */
void AddPrim(void *ot, void *p);
void ClearImage(RECT *rect, u_char r, u_char g, u_char b);
void DrawOTag(u_long *p);
int DrawSync(int mode);
int FntPrint();                       /* varargs (FntPrint(fmt, ...)) */
u_short GetClut(int x, int y);
int GetGraphType(void);
u_short GetTPage(int tp, int abr, int x, int y);
int LoadImage(RECT *rect, u_long *p);
int StoreImage(RECT *rect, u_long *p);
int MoveImage(RECT *rect, int x, int y);
void MargePrim(void *p0, void *p1);
DRAWENV *PutDrawEnv(DRAWENV *env);
int ResetGraph(int mode);
DRAWENV *SetDefDrawEnv(DRAWENV *env, int x, int y, int w, int h);
void SetDrawMode(DR_MODE *p, int dfe, int dtd, int tpage, RECT *tw);
int SetGraphDebug(int level);
void SetPolyF4(POLY_F4 *p);
void SetPolyFT4(POLY_FT4 *p);
void SetPolyGT3(POLY_GT3 *p);
void SetSemiTrans(void *p, int abe);
void SetTile(TILE *p);
/* ---------------------------------------------------------------- libgs */
void GsClearDispArea(u_char r, u_char g, u_char b);
void GsClearOt(u_short offset, u_short point, GsOT *otp);
void GsDefDispBuff(u_short x0, u_short y0, u_short x1, u_short y1);
int GsGetActiveBuff(void);
void GsGetLs(GsCOORDINATE2 *coord, MATRIX *m);
void GsGetTimInfo(u_long *im, GsIMAGE *tim);
void GsInit3D(void);
void GsInitCoordinate2(GsCOORDINATE2 *super, GsCOORDINATE2 *base);
void GsInitGraph(u_short x, u_short y, u_short intmode, u_short dith, u_short varmmode);
void GsInitGraph2(u_short x, u_short y, u_short intmode, u_short dith, u_short varmmode);
void GsLinkObject5(u_long tmd_base, GsDOBJ5 *objp, int n);
void GsMapModelingData(u_long *p);
void GsMulCoord0(MATRIX *m1, MATRIX *m2, MATRIX *m3);
void GsMulCoord2(MATRIX *m1, MATRIX *m2);
void GsMulCoord3(MATRIX *m1, MATRIX *m2);
u_long *GsPresetObject(GsDOBJ5 *objp, u_long *base_addr);
void GsSetAmbient(long r, long g, long b);
int GsSetFlatLight(int id, GsF_LIGHT *lt);
void GsSetLightMatrix(MATRIX *mp);
void GsSetLightMode(int mode);
void GsSetLsMatrix(MATRIX *mp);
void GsSetOffset(long offx, long offy);
int GsSetRefView2(GsRVIEW2 *pv);
void GsSortBoxFill(GsBOXF *bp, GsOT *otp, u_short pri);
void GsSortClear(u_char r, u_char g, u_char b, GsOT *otp);
void GsSortFastSprite(GsSPRITE *sp, GsOT *otp, u_short pri);
void GsSortFlipSprite(GsSPRITE *sp, GsOT *otp, u_short pri);
void GsSortGLine(GsGLINE *lp, GsOT *otp, u_short pri);
void GsSortLine(GsLINE *lp, GsOT *otp, u_short pri);
void GsSortObject5(GsDOBJ5 *objp, GsOT *otp, int shift, u_long *scratch);
void GsSortSprite(GsSPRITE *sp, GsOT *otp, u_short pri);
void GsSwapDispBuff(void);
void GsPrstTG3L();                    /* internal TMD packet routines, called via tables */
void GsPrstTG3NL();
/* ---------------------------------------------------------------- libetc / libapi / libc */
int VSync(int mode);
void *ResetCallback(void);
long EnterCriticalSection(void);
void ExitCriticalSection(void);
long OpenEvent(u_long desc, long spec, long mode, long (*func)());
long EnableEvent(long event);
long TestEvent(long event);
u_long OpenTh(long (*func)(), u_long sp, u_long gp);
int CloseTh(u_long thread);
int ChangeTh(u_long thread);
u_long GetGp(void);
long SetRCnt(u_long spec, u_short target, long mode);
long StartRCnt(u_long spec);
struct DIRENTRY *firstfile_FIRST(char *name, struct DIRENTRY *dir);   /* firstfile() */
struct DIRENTRY *nextfile(struct DIRENTRY *dir);
long open(char *devname, int flag);
long close(long fd);
long read(long fd, void *buf, long n);
long write(long fd, void *buf, long n);
long format(char *fs);
long rename(char *src, char *dest);
void _bu_init(void);
long _card_info(long chan);
long _card_load(long chan);
long _card_clear(long chan);
void InitCARD_INIT(long val);   /* InitCARD() (LIBCARD) */
long StartCARD_INIT(void);      /* StartCARD() */

int rand(void);
void srand(unsigned int seed);
void *memset_C43(void *s, int c, int n);   /* memset() */
char *strcpy(char *dst, const char *src);
int strcmp(const char *a, const char *b);
int printf();                   /* varargs */
int sprintf();                  /* varargs */

/* ---------------------------------------------------------------- libcd */
int CdInit(void);
int CdReset(int mode);
int CdControl(u_char com, u_char *param, u_char *result);
int CdControlB(u_char com, u_char *param, u_char *result);
int CdSync(int mode, u_char *result);
int CdReadSync(int mode, u_char *result);
int CdRead(int sectors, u_long *buf, int mode);
int CdRead2(long mode);
CdlLOC *CdIntToPos(int i, CdlLOC *p);
int CdPosToInt(CdlLOC *p);
CdlFILE *CdSearchFile(CdlFILE *fp, char *name);
int CdMix(u_char *vol);
void *CdDataCallback(void (*func)());
void *CdReadyCallback(void (*func)());
void StCdInterrupt(void);
void StSetRing(u_long *ring_addr, u_long ring_size);
void StClearRing(void);
void StUnSetRing(void);
void StSetStream(u_long mode, u_long start_frame, u_long end_frame,
                 void (*func1)(), void (*func2)());
u_long StGetNext(u_long **addr, u_long **header);
u_long StFreeRing(u_long *base);
/* ---------------------------------------------------------------- libpress (MDEC) */
void DecDCTReset(int mode);
int DecDCTvlc(u_long *bs, u_long *buf);
void DecDCTin(u_long *buf, int mode);
void DecDCTout(u_long *buf, int size);
int DecDCToutSync(int mode);
int DecDCToutCallback(void (*func)());
/* ---------------------------------------------------------------- libsnd / libspu */
void SsInit(void);
void SsStart(void);
void SsEnd(void);
void SsQuit(void);
void SsSetTableSize(char *table, short s_max, short t_max);
void SsSetTickMode(long tick_mode);
void SsSetMVol(short voll, short volr);
void SsGetMVol();               /* SsGetMVol(SndVolume *) */
void SsSetRVol(short voll, short volr);
void SsSetMono(void);
void SsSetStereo(void);
void SsSetSerialAttr(char s_num, char attr, char mode);
void SsSetSerialVol(char s_num, short voll, short volr);
short SsVabOpenHeadSticky(u_char *addr, short vabid, u_long sbaddr);
short SsVabTransBody(u_char *addr, short vabid);
short SsVabTransCompleted(short immediateFlag);
void SsVabClose(short vab_id);
short SsSepOpen(u_long *addr, short vab_id, short seq_cnt);
void SsSepClose(short sep_access_num);
void SsSepPlay(short sep_access_num, short seq_num, char play_mode, short l_count);
void SsSepStop(short sep_access_num, short seq_num);
void SsSepReplay(short sep_access_num, short seq_num);
void SsSepSetVol(short sep_access_num, short seq_num, short voll, short volr);
void SsSepSetCrescendo(short sep_access_num, short seq_num, short v, long t);
short SsIsEos(short access_num, short seq_num);
void SsUtReverbOn(void);
void SsUtReverbOff(void);
short SsUtSetReverbType(short type);
void SsUtSetReverbDepth(short ldepth, short rdepth);
void SpuSetKey(long on_off, u_long voice_bit);
void SpuGetAllKeysStatus(char *status);
long SpuClearReverbWorkArea(long rev_mode);
/* ---------------------------------------------------------------- libpad (Dual Shock) */
void PadInitDirect(u_char *pad1, u_char *pad2);
void PadStartCom_PADENTRY(void);           /* PadStartCom() */
int PadGetState(int port);
int PadInfoMode(int port, int term, int offs);
int PadSetMainMode(int socket, int offs, int lock);
void PadSetAct(int port, u_char *table, int len);
int PadSetActAlign(int port, u_char *table);
/* ---------------------------------------------------------------- libsn (dev-kit host files) */
int PCopen(char *name, int flags, int perms);
int PCclose(int fd);
int PCread(int fd, char *buff, int len);
int PClseek(int fd, int offset, int mode);

#endif /* PSYQ_PROTOS_H */
