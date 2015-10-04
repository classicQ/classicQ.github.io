struct qtvr;

char *QTVR_Get_Retval(struct qtvr *qtvr);
int QTVR_Waiting(struct qtvr *qtvr);
struct qtvr *QTVR_Create(const char *server, const char *request);
void QTVR_Destroy(struct qtvr *qtvr);

