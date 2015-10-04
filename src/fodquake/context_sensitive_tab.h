#define CSTC_NO_INPUT					( 1 << 0)
#define CSTC_COLOR_SELECTOR				( 1 << 1)
#define CSTC_PLAYER_COLOR_SELECTOR		( 1 << 2)
#define CSTC_MULTI_COMMAND				( 1 << 3)
#define CSTC_SLIDER						( 1 << 4)
#define CSTC_EXECUTE					( 1 << 5)
#define CSTC_COMMAND					( 1 << 6)
#define CSTC_HIGLIGHT_INPUT				( 1 << 7)


#define INPUT_MAX 512
struct cst_info
{
	char *name;
	char real_name[INPUT_MAX];	//for multicmd stuff
	struct tokenized_string *commands;	// tokenized commands if multicmd
	char input[INPUT_MAX];
	int argument_start;
	int argument_length;
	int command_start;
	int command_length;
	int results;	// overall results count
	struct tokenized_string *tokenized_input;	// tokenized input
	int flags;
	char *tooltip;
	qboolean tooltip_show;

	cmd_function_t *function;
	cvar_t *variable;

	int (*result)(struct cst_info *self, int *results, int get_result, int result_type, char **result);
	int (*get_data)(struct cst_info *self, int remove);
	void (*draw)(struct cst_info *self);	// more fitting name...

	struct input *new_input;

	// Variables of interest to noninternal functions
	
//	qboolean initialized;	// not touched by any internal function

	// set by the internal key function
	int selection;					// current selection
	qboolean input_changed;			// will be set if the input changed
	qboolean toggleables[12];		// will be toggled by ctrl + 1->0
	qboolean toggleables_changed;	// will be set if one of the toggleables changed
	qboolean selection_changed;		// will be set if the selection changed

	// set by the internal draw function
	int direction;		// draw direction 1 = up / -1 = down
	int rows;			// available rows
	// offset of the actual selection on screen
	int offset_x;			// x drawing offset
	int offset_y;			// y drawing offset

	void *data;		// has to be cleared by get_data()

	// used by command completion
	qboolean bool_var[5];
	int int_var[5];
	double double_var[5];
};

enum cstc_result_type
{
	cstc_rt_real,
	cstc_rt_draw,
	cstc_rt_highlight
};

void CSTC_Add(char *name, int (*conditions)(void), int (*result)(struct cst_info *self, int *results, int get_result, int result_type, char **result), int (*get_data)(struct cst_info *self, int remove), void (*draw)(struct cst_info *self), int flags, char *tooltip);
void CSTC_Insert_And_Close(void);
void Context_Sensitive_Tab_Completion_CvarInit(void);
void Context_Weighting_Init(void);
void Context_Weighting_Shutdown(void);
void Context_Sensitive_Tab_Completion_Draw(void);
void Context_Sensitive_Tab_Completion_Key(int key);
int Context_Sensitive_Tab_Completion(void);
void CSTC_Console_Close(void);
void CSTC_Shutdown(void);
void CSTC_PictureShutdown(void);
void CSTC_PictureInit(void);
void Context_Sensitive_Tab_Completion_Notification(qboolean input);
