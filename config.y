%{

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "config.h"
#include "functions.h"
#include "state.h"
#include "utils.h"
#include "queue.h"

#define MAXPUSHBACK 128
#define YYSTYPE_IS_DECLARED

typedef struct file_t file_t;
typedef struct keywords_t keywords_t;

static const struct {
	char ch;
	int mask;
} bind_mods[] = {
	{ 'S',  ShiftMask },
	{ 'C',  ControlMask },
	{ 'M',  Mod1Mask },
	{ '4',  Mod4Mask },
	{ '5',  Mod5Mask },
};

struct file_t {
	TAILQ_ENTRY(file_t) entry;

	FILE *stream;
	char *name;
	int lineno;
	int errors;
};

struct keywords_t {
	char *k_name;
	int k_val;
};

static const struct {
	char *tag;
	void (*function)(state_t *, void *, long);
	binding_context_t context;
	long flag;
} name_to_func[] = {
#define FUNC_CC(t, h, n) #t, function_ ## h, BINDING_CONTEXT_CLIENT, n
#define FUNC_SC(t, h, n) #t, function_ ## h, BINDING_CONTEXT_SCREEN, n
#define FUNC_GC(t, h, n) #t, function_ ## h, BINDING_CONTEXT_GLOBAL, n
	{ FUNC_SC(group-cycle, group_cycle, 0) },
	{ FUNC_SC(menu-command, menu_command, 0) },
	{ FUNC_SC(menu-exec, menu_exec, 0) },
	{ FUNC_CC(menu-windows, menu_windows, 0) },
	{ FUNC_GC(terminal, terminal, 0) },
	{ FUNC_CC(window-center, window_center, 0) },
	{ FUNC_CC(window-close, window_close, 0) },
	{ FUNC_CC(window-cycle, window_cycle, 0) },
	{ FUNC_CC(window-fullscreen, window_fullscreen, 0) },
	{ FUNC_CC(window-hide, window_hide, 0) },
	{ FUNC_CC(window-maximize, window_maximize, 0) },
	{ FUNC_CC(window-rcycle, window_cycle, 1) },
	{ FUNC_CC(window-move, window_move, 0) },
	{ FUNC_CC(window-move-to-screen-above, window_move_to_screen, DIRECTION_UP) },
	{ FUNC_CC(window-move-to-screen-below, window_move_to_screen, DIRECTION_DOWN) },
	{ FUNC_CC(window-move-to-screen-left, window_move_to_screen, DIRECTION_LEFT) },
	{ FUNC_CC(window-move-to-screen-right, window_move_to_screen, DIRECTION_RIGHT) },
	{ FUNC_CC(window-resize, window_resize, 0) },
	{ FUNC_CC(window-restore, window_restore, 0) },
	{ FUNC_CC(window-tile-down, window_tile, DIRECTION_DOWN) },
	{ FUNC_CC(window-tile-down-left, window_tile, DIRECTION_DOWN | DIRECTION_LEFT) },
	{ FUNC_CC(window-tile-down-left-third, window_tile, DIRECTION_DOWN | DIRECTION_LEFT_THIRD) },
	{ FUNC_CC(window-tile-down-right, window_tile, DIRECTION_DOWN | DIRECTION_RIGHT) },
	{ FUNC_CC(window-tile-down-right-third, window_tile, DIRECTION_DOWN | DIRECTION_RIGHT_THIRD) },
	{ FUNC_CC(window-tile-down-third, window_tile, DIRECTION_DOWN_THIRD) },
	{ FUNC_CC(window-tile-down-third-left, window_tile, DIRECTION_DOWN_THIRD | DIRECTION_LEFT) },
	{ FUNC_CC(window-tile-down-third-left-third, window_tile, DIRECTION_DOWN_THIRD | DIRECTION_LEFT_THIRD) },
	{ FUNC_CC(window-tile-down-third-right, window_tile, DIRECTION_DOWN_THIRD | DIRECTION_RIGHT) },
	{ FUNC_CC(window-tile-down-third-right-third, window_tile, DIRECTION_DOWN_THIRD | DIRECTION_RIGHT_THIRD) },
	{ FUNC_CC(window-tile-left, window_tile, DIRECTION_LEFT) },
	{ FUNC_CC(window-tile-left-third, window_tile, DIRECTION_LEFT_THIRD) },
	{ FUNC_CC(window-tile-right, window_tile, DIRECTION_RIGHT) },
	{ FUNC_CC(window-tile-right-third, window_tile, DIRECTION_RIGHT_THIRD) },
	{ FUNC_CC(window-tile-up, window_tile, DIRECTION_UP) },
	{ FUNC_CC(window-tile-up-left, window_tile, DIRECTION_UP | DIRECTION_LEFT) },
	{ FUNC_CC(window-tile-up-left-third, window_tile, DIRECTION_UP | DIRECTION_LEFT_THIRD) },
	{ FUNC_CC(window-tile-up-right, window_tile, DIRECTION_UP | DIRECTION_RIGHT) },
	{ FUNC_CC(window-tile-up-right-third, window_tile, DIRECTION_UP | DIRECTION_RIGHT_THIRD) },
	{ FUNC_CC(window-tile-up-third, window_tile, DIRECTION_UP_THIRD) },
	{ FUNC_CC(window-tile-up-third-left, window_tile, DIRECTION_UP_THIRD | DIRECTION_LEFT) },
	{ FUNC_CC(window-tile-up-third-left-third, window_tile, DIRECTION_UP_THIRD | DIRECTION_LEFT_THIRD) },
	{ FUNC_CC(window-tile-up-third-right, window_tile, DIRECTION_UP_THIRD | DIRECTION_RIGHT) },
	{ FUNC_CC(window-tile-up-third-right-third, window_tile, DIRECTION_UP_THIRD | DIRECTION_RIGHT_THIRD) },
	{ FUNC_GC(quit, wm_state, 3) },
	{ FUNC_GC(restart, wm_state, 2) },
#undef FUNC_CC
#undef FUNC_SC
#undef FUNC_GC
};

TAILQ_HEAD(files, file_t) files = TAILQ_HEAD_INITIALIZER(files);

typedef struct {
    union {
        int64_t number;
        char *string;
    } v;
    int lineno;
} YYSTYPE;

char *config_bind_mask(char *, unsigned int *);
int findeol(void);
int kw_cmp(const void *, const void *);
int lgetc(int);
int lookup(char *);
int lungetc(int);
int popfile(void);
file_t *pushfile(char *, FILE *);
int yyerror(char *, ...)
	__attribute__((__format__ (printf, 1, 2)))
	__attribute__((__nonnull__ (1)));
int yylex(void);
int yyparse(void);

char *parsebuf, pushback_buffer[MAXPUSHBACK];
int parseindex, pushback_index = 0;
file_t *file;
file_t *topfile;
static config_t *config;

%}

%token APPLICATIONS
%token BINDKEY
%token BINDMOUSE
%token BORDERACTIVE
%token BORDERINACTIVE
%token BORDERMARK
%token BORDERURGENT
%token BORDERWIDTH
%token CASCADE
%token COLOR
%token COMMAND
%token ERROR
%token FONT
%token IGNORE
%token LABEL
%token MENUBACKGROUND
%token MENUFOREGROUND
%token MENUINPUT
%token MENUITEM
%token MENUITEMDETAIL
%token MENUPROMPT
%token MENUSELECTIONBACKGROUND
%token MENUSELECTIONFOREGROUND
%token MENUSEPARATOR
%token NO
%token POINTER
%token RUN
%token TRANSITIONDURATION
%token WINDOWACTIVE
%token WINDOWHIDDEN
%token WINDOWINACTIVE
%token WINDOWPLACEMENT
%token WINDOWS
%token WMNAME
%token YES

%token <v.number> NUMBER
%token <v.string> STRING

%type <v.string> string
%type <v.number> yesno

%%

grammar	:
		| grammar '\n'
		| grammar main '\n'
		| grammar color '\n'
		| grammar font '\n'
		| grammar label '\n'
		;

color	: COLOR colors
		;
colors	: BORDERACTIVE STRING {
			 free(config->colors[COLOR_BORDER_ACTIVE]);
			 config->colors[COLOR_BORDER_ACTIVE] = $2;
		}
		| BORDERINACTIVE STRING {
			 free(config->colors[COLOR_BORDER_INACTIVE]);
			 config->colors[COLOR_BORDER_INACTIVE] = $2;
		}
		| BORDERMARK STRING {
			free(config->colors[COLOR_BORDER_MARK]);
			config->colors[COLOR_BORDER_MARK] = $2;
		}
		| BORDERURGENT STRING {
			 free(config->colors[COLOR_BORDER_URGENT]);
			 config->colors[COLOR_BORDER_URGENT] = $2;
		}
		| BORDERWIDTH NUMBER {
			 config->border_width = $2;
		}
		| MENUBACKGROUND STRING {
			 free(config->colors[COLOR_MENU_BACKGROUND]);
			 config->colors[COLOR_MENU_BACKGROUND] = $2;
		}
		| MENUFOREGROUND STRING {
			 free(config->colors[COLOR_MENU_FOREGROUND]);
			 config->colors[COLOR_MENU_FOREGROUND] = $2;
		}
		| MENUPROMPT STRING {
			 free(config->colors[COLOR_MENU_PROMPT]);
			 config->colors[COLOR_MENU_PROMPT] = $2;
		}
		| MENUSELECTIONBACKGROUND STRING {
			 free(config->colors[COLOR_MENU_SELECTION_BACKGROUND]);
			 config->colors[COLOR_MENU_SELECTION_BACKGROUND] = $2;
		}
		| MENUSELECTIONFOREGROUND STRING {
			 free(config->colors[COLOR_MENU_SELECTION_FOREGROUND]);
			 config->colors[COLOR_MENU_SELECTION_FOREGROUND] = $2;
		}
		| MENUSEPARATOR STRING {
			free(config->colors[COLOR_MENU_SEPARATOR]);
			config->colors[COLOR_MENU_SEPARATOR] = $2;
		}
		;

font	: FONT fonts
		;
fonts	: MENUINPUT STRING {
			 free(config->fonts[FONT_MENU_INPUT]);
			 config->fonts[FONT_MENU_INPUT] = $2;
		}
		| MENUITEM STRING {
			 free(config->fonts[FONT_MENU_ITEM]);
			 config->fonts[FONT_MENU_ITEM] = $2;
		}
		| MENUITEMDETAIL STRING {
			 free(config->fonts[FONT_MENU_ITEM_DETAIL]);
			 config->fonts[FONT_MENU_ITEM_DETAIL] = $2;
		}
		;

label	: LABEL labels
		;

labels	: APPLICATIONS STRING {
			if (config->labels[LABEL_APPLICATIONS]) {
				free(config->labels[LABEL_APPLICATIONS]);
			}

			config->labels[LABEL_APPLICATIONS] = strdup($2);
			free($2);
		}
		| RUN STRING {
			if (config->labels[LABEL_RUN]) {
				free(config->labels[LABEL_RUN]);
			}

			config->labels[LABEL_RUN] = strdup($2);
			free($2);
		}
		| WINDOWS STRING {
			if (config->labels[LABEL_WINDOWS]) {
				free(config->labels[LABEL_WINDOWS]);
			}

			config->labels[LABEL_WINDOWS] = strdup($2);
			free($2);
		}
		| WINDOWACTIVE STRING {
			if (config->labels[LABEL_WINDOW_ACTIVE]) {
				free(config->labels[LABEL_WINDOW_ACTIVE]);
			}

			config->labels[LABEL_WINDOW_ACTIVE] = strdup($2);
			free($2);
		}
		| WINDOWINACTIVE STRING {
			if (config->labels[LABEL_WINDOW_INACTIVE]) {
				free(config->labels[LABEL_WINDOW_INACTIVE]);
			}

			config->labels[LABEL_WINDOW_INACTIVE] = strdup($2);
			free($2);
		}
		| WINDOWHIDDEN STRING {
			if (config->labels[LABEL_WINDOW_HIDDEN]) {
				free(config->labels[LABEL_WINDOW_HIDDEN]);
			}

			config->labels[LABEL_WINDOW_HIDDEN] = strdup($2);
			free($2);
		}
		;

string	: string STRING {
			 if (xasprintf(&$$, "%s %s", $1, $2) == -1) {
				 free($1);
				 free($2);
				 yyerror("string: asprintf");
				 YYERROR;
			 }
			 free($1);
			 free($2);
		}
		| STRING
		;

yesno	: YES	{ $$ = 1; }
		| NO	{ $$ = 0; }
		;

main	: BINDKEY STRING string {
			 if (!config_bind_key(config, $2, $3)) {
				 yyerror("invalid bind-key: %s %s", $2, $3);
				 free($2);
				 free($3);
				 YYERROR;
			 }
			 free($2);
			 free($3);
		}
		| BINDMOUSE STRING string {
			 if (!config_bind_mouse(config, $2, $3)) {
				 yyerror("invalid bind-mouse: %s %s", $2, $3);
				 free($2);
				 free($3);
				 YYERROR;
			 }
			 free($2);
			 free($3);
		}
		| BORDERWIDTH NUMBER {
			 config->border_width = $2;
		}
		| COMMAND STRING string {
			 if (strlen($3) >= PATH_MAX) {
				 yyerror("%s command path too long", $2);
				 free($2);
				 free($3);
				 YYERROR;
			 }
			 config_bind_command(config, $2, $3);
			 free($2);
			 free($3);
		}
		| IGNORE STRING {
			config_ignore(config, $2);
			free($2);
		}
		| TRANSITIONDURATION NUMBER {
			config->transition_duration = (double)$2 / 1000.0;
		}
		| WINDOWPLACEMENT CASCADE {
			config->window_placement = WINDOW_PLACEMENT_CASCADE;
		}
		| WINDOWPLACEMENT POINTER {
			config->window_placement = WINDOW_PLACEMENT_POINTER;
		}
		| WMNAME STRING {
			free(config->wm_name);
			config->wm_name = strdup($2);
			free($2);
		}
		;

%%

int
findeol(void)
{
	int	c;

	parsebuf = NULL;

	while (1) {
		if (pushback_index)
			c = pushback_buffer[--pushback_index];
		else
			c = lgetc(0);
		if (c == '\n') {
			file->lineno++;
			break;
		}
		if (c == EOF)
			break;
	}
	return (ERROR);
}

int
kw_cmp(const void *k, const void *e)
{
	return (strcmp(k, ((keywords_t *)e)->k_name));
}

int
lgetc(int quotec)
{
	int c, next;

if (parsebuf) {
		if (parseindex >= 0) {
			c = parsebuf[parseindex++];
			if (c != '\0')
				return (c);
			parsebuf = NULL;
		} else
			parseindex++;
	}

if (pushback_index)
		return (pushback_buffer[--pushback_index]);

if (quotec) {
		if ((c = getc(file->stream)) == EOF) {
			yyerror("reached end of file while parsing quoted string");
			if (file == topfile || popfile() == EOF)
				return (EOF);
			return (quotec);
		}
		return (c);
	}

while ((c = getc(file->stream)) == '\\') {
		next = getc(file->stream);
		if (next != '\n') {
			c = next;
			break;
		}
		yylval.lineno = file->lineno;
		file->lineno++;
	}

while (c == EOF) {
		if (file == topfile || popfile() == EOF)
			return (EOF);
		c = getc(file->stream);
	}
	return (c);
}

int
lookup(char *s)
{
	static const keywords_t keywords[] = {
		{ "applications", APPLICATIONS },
		{ "bind-key", BINDKEY },
		{ "bind-mouse", BINDMOUSE },
		{ "border-active", BORDERACTIVE },
		{ "border-inactive", BORDERINACTIVE },
		{ "border-mark", BORDERMARK },
		{ "border-urgent", BORDERURGENT },
		{ "border-width", BORDERWIDTH },
		{ "cascade", CASCADE },
		{ "color", COLOR },
		{ "command", COMMAND },
		{ "font", FONT },
		{ "ignore", IGNORE },
		{ "label", LABEL },
		{ "menu-background", MENUBACKGROUND },
		{ "menu-foreground", MENUFOREGROUND },
		{ "menu-input", MENUINPUT },
		{ "menu-item", MENUITEM },
		{ "menu-item-detail", MENUITEMDETAIL },
		{ "menu-prompt", MENUPROMPT },
		{ "menu-selection-background", MENUSELECTIONBACKGROUND },
		{ "menu-selection-foreground", MENUSELECTIONFOREGROUND },
		{ "menu-separator", MENUSEPARATOR },
		{ "no", NO },
		{ "pointer", POINTER },
		{ "run", RUN },
		{ "transition-duration", TRANSITIONDURATION },
		{ "window-active", WINDOWACTIVE },
		{ "window-inactive", WINDOWINACTIVE },
		{ "window-hidden", WINDOWHIDDEN },
		{ "window-placement", WINDOWPLACEMENT },
		{ "windows", WINDOWS },
		{ "wm-name", WMNAME },
		{ "yes", YES }
	};
	keywords_t *p;

	p = bsearch(s, keywords, sizeof(keywords) / sizeof(keywords[0]),
	    sizeof(keywords[0]), kw_cmp);

	if (p)
		return (p->k_val);
	else
		return (STRING);
}

int
lungetc(int c)
{
	if (c == EOF)
		return (EOF);
	if (parsebuf) {
		parseindex--;
		if (parseindex >= 0)
			return (c);
	}
	if (pushback_index < MAXPUSHBACK-1)
		return (pushback_buffer[pushback_index++] = c);
	else
		return (EOF);
}

int
popfile(void)
{
	file_t *prev;

	if ((prev = TAILQ_PREV(file, files, entry)) != NULL)
		prev->errors += file->errors;

	TAILQ_REMOVE(&files, file, entry);
	fclose(file->stream);
	free(file->name);
	free(file);
	file = prev;
	return (file ? 0 : EOF);
}

file_t *
pushfile(char *name, FILE *stream)
{
	file_t *nfile;

	nfile = malloc(sizeof(file_t));
	nfile->name = strdup(name);
	nfile->stream = stream;
	nfile->lineno = 1;
	nfile->errors = 0;
	TAILQ_INSERT_TAIL(&files, nfile, entry);
	return (nfile);
}

int
yyerror(char *fmt, ...)
{
	va_list ap;

	file->errors++;
	va_start(ap, fmt);
	fprintf(stderr, "%s:%d: ", file->name, yylval.lineno);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	va_end(ap);
	return (0);
}

int
yylex(void)
{
	char buf[8096], *p;
	int c, next, token, quotec;

	p = buf;
	while ((c = lgetc(0)) == ' ' || c == '\t');

	yylval.lineno = file->lineno;
	if (c == '#')
		while ((c = lgetc(0)) != '\n' && c != EOF);

	switch (c) {
		case '\'':
		case '"':
			quotec = c;
			while (1) {
				if ((c = lgetc(quotec)) == EOF)
					return (0);
				if (c == '\n') {
					file->lineno++;
					continue;
				} else if (c == '\\') {
					if ((next = lgetc(quotec)) == EOF)
						return (0);
					if (next == quotec || next == ' ' ||
						next == '\t')
						c = next;
					else if (next == '\n') {
						file->lineno++;
						continue;
					} else
						lungetc(next);
				} else if (c == quotec) {
					*p = '\0';
					break;
				} else if (c == '\0') {
					yyerror("syntax error");
					return (findeol());
				}
				if (p + 1 >= buf + sizeof(buf) - 1) {
					yyerror("string too long");
					return (findeol());
				}
				*p++ = (char)c;
			}
			yylval.v.string = strdup(buf);
			return (STRING);
	}

#define allowed_to_end_number(x) \
	(isspace(x) || x == ')' || x ==',' || x == '/' || x == '}' || x == '=')

	if (c == '-' || isdigit(c)) {
		do {
			*p++ = c;
			if ((unsigned)(p-buf) >= sizeof(buf)) {
				yyerror("string too long");
				return (findeol());
			}
		} while ((c = lgetc(0)) != EOF && isdigit(c));
		lungetc(c);
		if (p == buf + 1 && buf[0] == '-')
			goto nodigits;
		if (c == EOF || allowed_to_end_number(c)) {
			char *errstr = NULL;

			*p = '\0';
			yylval.v.number = strtonum(buf, LLONG_MIN,
				LLONG_MAX, &errstr);
			if (errstr) {
				yyerror("\"%s\" invalid number: %s",
					buf, errstr);
				return (findeol());
			}
			return (NUMBER);
		} else {
nodigits:
		while (p > buf + 1)
				lungetc(*--p);
			c = *--p;
			if (c == '-')
				return (c);
		}
	}

#define allowed_in_string(x) \
	(isalnum(x) || (ispunct(x) && x != '(' && x != ')' && \
	x != '{' && x != '}' && x != '<' && x != '>' && \
	x != '!' && x != '=' && x != '#' && x != ','))

	if (isalnum(c) || c == ':' || c == '_' || c == '*' || c == '/') {
		do {
			*p++ = c;
			if ((unsigned)(p-buf) >= sizeof(buf)) {
				yyerror("string too long");
				return (findeol());
			}
		} while ((c = lgetc(0)) != EOF && (allowed_in_string(c)));
		lungetc(c);
		*p = '\0';
		if ((token = lookup(buf)) == STRING)
			yylval.v.string = strdup(buf);
		return (token);
	}
	if (c == '\n') {
		yylval.lineno = file->lineno;
		file->lineno++;
	}
	if (c == EOF)
		return (0);
	return (c);
}

void
config_bind_command(config_t *config, char *name, char *path)
{
	command_t *command, *current, *next;

	command = malloc(sizeof(command_t));
	command->name = strdup(name);
	command->path = strdup(path);

	TAILQ_FOREACH_SAFE(current, &config->commands, entry, next) {
		if (!strcmp(current->name, name)) {
			TAILQ_REMOVE(&config->commands, current, entry);
			free(current->name);
			free(current->path);
			free(current);
		}
	};

	TAILQ_INSERT_TAIL(&config->commands, command, entry);
}

Bool
config_bind_key(config_t *config, char *bind, char *cmd)
{
	char *key;
	int button, i, modifier;
	binding_t *binding;

	if (!cmd) {
		return False;
	}

	key = config_bind_mask(bind, &modifier);
	button = XStringToKeysym(key);

	if (button == NoSymbol) {
		return False;
	}

	TAILQ_FOREACH(binding, &config->keybindings, entry) {
		if (!strcmp(binding->name, cmd)) {
			binding->button = button;
			binding->modifier = modifier;
			return True;
		}
	}

	for (i = 0; i < sizeof(name_to_func) / sizeof(name_to_func[0]); i++) {
		if (!strcmp(name_to_func[i].tag, cmd)) {
			binding = malloc(sizeof(binding_t));
			binding->button = button;
			binding->modifier = modifier;
			binding->name = strdup(cmd);
			binding->function = name_to_func[i].function;
			binding->context = name_to_func[i].context;
			binding->flag = name_to_func[i].flag;
			TAILQ_INSERT_TAIL(&config->keybindings, binding, entry);
			return True;
		}
	}

	return False;
}

char *
config_bind_mask(char *name, unsigned int *mask)
{
	char *ch, *dash;
	unsigned int i;

	*mask = 0;
	if ((dash = strchr(name, '-')) == NULL)
		return name;

	for (i = 0; i < sizeof(bind_mods) / sizeof(bind_mods[0]); i++) {
		if ((ch = strchr(name, bind_mods[i].ch)) != NULL && ch < dash)
			*mask |= bind_mods[i].mask;
	}

	return dash + 1;
}

Bool
config_bind_mouse(config_t *config, char *bind, char *cmd)
{
	char *key, *errstr;
	int button, i, modifier;
	binding_t *binding;

	if (!cmd) {
		return False;
	}

	key = config_bind_mask(bind, &modifier);
	button = strtonum(key, Button1, Button5, &errstr);
	if (errstr) {
		return False;
	}

	TAILQ_FOREACH(binding, &config->mousebindings, entry) {
		if (!strcmp(binding->name, cmd)) {
			binding->button = button;
			binding->modifier = modifier;
			return True;
		}
	}

	for (i = 0; i < sizeof(name_to_func) / sizeof(name_to_func[0]); i++) {
		if (!strcmp(name_to_func[i].tag, cmd)) {
			binding = malloc(sizeof(binding_t));
			binding->button = button;
			binding->modifier = modifier;
			binding->name = strdup(cmd);
			binding->function = name_to_func[i].function;
			binding->context = name_to_func[i].context;
			binding->flag = name_to_func[i].flag;
			TAILQ_INSERT_TAIL(&config->mousebindings, binding, entry);
			return True;
		}
	}

	free(binding);

	return False;
}

void
config_free(config_t *config)
{
	command_t *command;
	binding_t *binding;
	int i;

	if (!config) {
		return;
	}

	while ((command = TAILQ_FIRST(&config->commands)) != NULL) {
		TAILQ_REMOVE(&config->commands, command, entry);
		free(command->name);
		free(command->path);
		free(command);
	}

	while ((binding = TAILQ_FIRST(&config->keybindings)) != NULL) {
		TAILQ_REMOVE(&config->keybindings, binding, entry);
		free(binding->name);
		free(binding);
	}

	while ((binding = TAILQ_FIRST(&config->mousebindings)) != NULL) {
		TAILQ_REMOVE(&config->mousebindings, binding, entry);
		free(binding->name);
		free(binding);
	}

	for (i = 0; i < FONT_NITEMS; i++)
		free(config->fonts[i]);

	free(config);
}

void
config_ignore(config_t *config, char *class_name)
{
	ignored_t *ignored;
	TAILQ_FOREACH(ignored, &config->ignored, entry) {
		if (!strcmp(ignored->class_name, class_name)) {
			return;
		}
	}

	ignored = calloc(1, sizeof(ignored_t));
	ignored->class_name = strdup(class_name);
	TAILQ_INSERT_TAIL(&config->ignored, ignored, entry);
}

config_t *
config_init()
{
	char path[BUFSIZ];
	FILE *stream;
	int errors = 0;

	config = malloc(sizeof(config_t));

	config->wm_name = strdup("MagnetWM");

	config->colors[COLOR_BORDER_ACTIVE] = strdup("green");
	config->colors[COLOR_BORDER_INACTIVE] = strdup("blue");
	config->colors[COLOR_BORDER_MARK] = strdup("magenta");
	config->colors[COLOR_BORDER_URGENT] = strdup("red");
	config->colors[COLOR_MENU_BACKGROUND] = strdup("black");
	config->colors[COLOR_MENU_FOREGROUND] = strdup("white");
	config->colors[COLOR_MENU_PROMPT] = strdup("darkgray");
	config->colors[COLOR_MENU_SELECTION_BACKGROUND] = strdup("blue");
	config->colors[COLOR_MENU_SELECTION_FOREGROUND] = strdup("white");
	config->colors[COLOR_MENU_SEPARATOR] = strdup("darkgray");

	config->fonts[FONT_MENU_INPUT] = strdup("sans-serif:pixelsize=14:bold");
	config->fonts[FONT_MENU_ITEM] = strdup("sans-serif:pixelsize=14:bold");
	config->fonts[FONT_MENU_ITEM_DETAIL] = strdup("sans-serif:pixelsize=14:italic");

	config->transition_duration = 0.0;
	config->border_width = 1;
	config->window_placement = WINDOW_PLACEMENT_CASCADE;

	config->labels[LABEL_APPLICATIONS] = strdup("Application");
	config->labels[LABEL_RUN] = strdup("Run");
	config->labels[LABEL_WINDOWS] = NULL;

	config->labels[LABEL_WINDOW_ACTIVE] = strdup("???");
	config->labels[LABEL_WINDOW_INACTIVE] = NULL;
	config->labels[LABEL_WINDOW_HIDDEN] = strdup("???");

	TAILQ_INIT(&config->commands);
	TAILQ_INIT(&config->keybindings);
	TAILQ_INIT(&config->mousebindings);
	TAILQ_INIT(&config->ignored);

	config_bind_command(config, "terminal", "xterm");

	config_bind_mouse(config, "M-1", "window-move");
	config_bind_mouse(config, "M-3", "window-resize");

	snprintf(path, BUFSIZ, "%s/.config/magnetwm/magnetwmrc", getenv("HOME"));
	stream = fopen(path, "r");
	if (stream == NULL) {
		snprintf(path, BUFSIZ, "%s/.magnetwmrc", getenv("HOME"));
		stream = fopen(path, "r");
		if (stream == NULL) {
			return config;
		}
	}

	file = pushfile(path, stream);
	topfile = file;

	yyparse();
	errors = file->errors;
	popfile();

	if (errors > 0) {
		fprintf(stderr, "Encountered %d errors\n", errors);
	}

	return config;
}
