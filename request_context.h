

struct RequestContext;
typedef struct RequestContext RequestContext;

struct RequestContext* init_request_context(int fd);

int rc_get_fd(struct RequestContext *context);

void destroy_request_context(struct RequestContext *context);
