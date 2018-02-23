
#include "quakedef.h" /* for frame_t :/ */

struct NetQW *NetQW_Create(const char *hoststring, const char *userinfo, unsigned short qport, unsigned int ftex);
void NetQW_Delete(struct NetQW *netqw);
void NetQW_GenerateFrames(struct NetQW *netqw);
void NetQW_SetFPS(struct NetQW *netqw, unsigned int fps);
unsigned long long NetQW_GetFrameTime(struct NetQW *netqw);
int NetQW_AppendReliableBuffer(struct NetQW *netqw, const void *buffer, unsigned int bufferlen);
unsigned int NetQW_GetPacketLength(struct NetQW *netqw);
void *NetQW_GetPacketData(struct NetQW *netqw);
void NetQW_FreePacket(struct NetQW *netqw);
void NetQW_CopyFrames(struct NetQW *netqw, frame_t *frames, unsigned int *newseqnr, unsigned int *startframe, unsigned int *endframe);
void NetQW_SetDeltaPoint(struct NetQW *netqw, int delta_sequence_number);
void NetQW_SetTeleport(struct NetQW *netqw, float *position);
void NetQW_SetLag(struct NetQW *netqw, unsigned int microseconds);
void NetQW_SetLagEzcheat(struct NetQW *netqw, int enabled);
unsigned long long NetQW_GetTimeSinceLastPacketFromServer(struct NetQW *netqw);
int NetQW_GetExtensions(struct NetQW *netqw, unsigned int *ftex);

void NetQW_LockMovement(struct NetQW *netqw);
void NetQW_UnlockMovement(struct NetQW *netqw);
void NetQW_SetForwardSpeed(struct NetQW *netqw, float value);
void NetQW_SetSideSpeed(struct NetQW *netqw, float value);
void NetQW_SetUpSpeed(struct NetQW *netqw, float value);
unsigned int NetQW_ButtonDown(struct NetQW *netqw, int button, int impulse);
void NetQW_ButtonUp(struct NetQW *netqw, int button);
void NetQW_SetImpulse(struct NetQW *netqw, int impulse);

