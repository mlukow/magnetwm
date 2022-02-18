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
/*
	{ "group-cycle", function_group_cycle, 0 },
	{ "group-rcycle", function_group_cycle, 1 },
	{ "firefox", function_firefox, 0 },
	*/
	{ FUNC_SC(group-cycle, group_cycle, 0) },
	{ FUNC_SC(menu-exec, menu_exec, 0) },
	{ FUNC_SC(menu-command, menu_command, 0) },
	{ FUNC_GC(terminal, terminal, 0) },
	{ FUNC_CC(window-center, window_center, 0) },
	{ FUNC_CC(window-cycle, window_cycle, 0) },
	{ FUNC_CC(window-rcycle, window_cycle, 1) },
	{ FUNC_CC(window-tile-up, window_tile, 1) },
	{ FUNC_CC(window-tile-up-right, window_tile, 9) },
	{ FUNC_CC(window-tile-right, window_tile, 8) },
	{ FUNC_CC(window-tile-down, window_tile, 2) },
	{ FUNC_CC(window-tile-down-right, window_tile, 10) },
	{ FUNC_CC(window-tile-down-left, window_tile, 6) },
	{ FUNC_CC(window-tile-left, window_tile, 4) },
	{ FUNC_CC(window-tile-up-left, window_tile, 5) },
	{ FUNC_CC(window-move, window_move, 0) },
	{ FUNC_CC(window-resize, window_resize, 0) },
};

TAILQ_HEAD(files, file_t) files = TAILQ_HEAD_INITIALIZER(files);

typedef struct {
    union {
        int64_t number;
        char *string;
    } v;
    int lineno;
} YYSTYPE;

void config_add_command(config_t *, char *, char *);
int config_bind_key(config_t *, char *, char *);
char *config_bind_mask(char *, unsigned int *);
int config_bind_mouse(config_t *, char *, char *);
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

%token BINDKEY
%token BINDMOUSE
%token BORDERACTIVE
%token BORDERINACTIVE
%token BORDERURGENT
%token BORDERWIDTH
%token COLOR
%token COMMAND
%token ERROR
%token FONT
%token MENUBACKGROUND
%token MENUFOREGROUND
%token MENUINPUT
%token MENUITEM
%token MENUPROMPT
%token MENUSELECTIONBACKGROUND
%token MENUSELECTIONFOREGROUND
%token MENUSEPARATOR
%token NO
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
			 config->colors[COLOR_MENU_BACKGROUND] = $2;
		}
		| MENUSELECTIONFOREGROUND STRING {
			 free(config->colors[COLOR_MENU_SELECTION_FOREGROUND]);
			 config->colors[COLOR_MENU_FOREGROUND] = $2;
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
		;

string	: string STRING {
			 if (asprintf(&$$, "%s %s", $1, $2) == -1) {
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
			 config_add_command(config, $2, $3);
			 free($2);
			 free($3);
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
		{ "bind-key", BINDKEY },
		{ "bind-mouse", BINDMOUSE },
		{ "border-active", BORDERACTIVE },
		{ "border-inactive", BORDERINACTIVE },
		{ "border-urgent", BORDERURGENT },
		{ "border-width", BORDERWIDTH },
		{ "color", COLOR },
		{ "command", COMMAND },
		{ "font", FONT },
		{ "menu-background", MENUBACKGROUND },
		{ "menu-foreground", MENUFOREGROUND },
		{ "menu-input", MENUINPUT },
		{ "menu-item", MENUITEM },
		{ "menu-prompt", MENUPROMPT },
		{ "menu-selection-background", MENUSELECTIONFOREGROUND },
		{ "menu-selection-foreground", MENUSELECTIONFOREGROUND },
		{ "menu-separator", MENUSEPARATOR },
		{ "no", NO },
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
config_add_command(config_t *config, char *name, char *path)
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

int
config_bind_key(config_t *config, char *bind, char *cmd)
{
	char *key;
	int i;
	binding_t *binding;

	if (!cmd)
		return 0;

	binding = malloc(sizeof(binding_t));
	key = config_bind_mask(bind, &binding->modifier);
	binding->button = XStringToKeysym(key);
	if (binding->button == NoSymbol) {
		free(binding);
		return 0;
	}

	for (i = 0; i < sizeof(name_to_func) / sizeof(name_to_func[0]); i++) {
		if (!strcmp(name_to_func[i].tag, cmd)) {
			binding->function = name_to_func[i].function;
			binding->context = name_to_func[i].context;
			binding->flag = name_to_func[i].flag;
			TAILQ_INSERT_TAIL(&config->keybindings, binding, entry);
			return 1;
		}
	}

	free(binding);

	return 0;
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

int config_bind_mouse(config_t *config, char *bind, char *cmd)
{
	char *button, *errstr;
	int i;
	binding_t *binding;

	if (!cmd)
		return 0;

	binding = malloc(sizeof(binding_t));
	button = config_bind_mask(bind, &binding->modifier);
	binding->button = strtonum(button, Button1, Button5, &errstr);
	if (errstr) {
		free(binding);
		return 0;
	}

	for (i = 0; i < sizeof(name_to_func) / sizeof(name_to_func[0]); i++) {
		if (!strcmp(name_to_func[i].tag, cmd)) {
			binding->function = name_to_func[i].function;
			binding->context = name_to_func[i].context;
			binding->flag = name_to_func[i].flag;
			TAILQ_INSERT_TAIL(&config->mousebindings, binding, entry);
			return 1;
		}
	}

	free(binding);

	return 0;
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
		free(binding);
	}

	while ((binding = TAILQ_FIRST(&config->mousebindings)) != NULL) {
		TAILQ_REMOVE(&config->mousebindings, binding, entry);
		free(binding);
	}

	for (i = 0; i < FONT_NITEMS; i++)
		free(config->fonts[i]);

	free(config);
}

config_t *
config_init(char *path)
{
	FILE *stream;
	int errors = 0;

	config = malloc(sizeof(config_t));

	config->colors[COLOR_BORDER_ACTIVE] = strdup("green");
	config->colors[COLOR_BORDER_INACTIVE] = strdup("blue");
	config->colors[COLOR_BORDER_URGENT] = strdup("red");
	config->colors[COLOR_MENU_BACKGROUND] = strdup("black");
	config->colors[COLOR_MENU_FOREGROUND] = strdup("white");
	config->colors[COLOR_MENU_PROMPT] = strdup("darkgray");
	config->colors[COLOR_MENU_SELECTION_BACKGROUND] = strdup("blue");
	config->colors[COLOR_MENU_SELECTION_FOREGROUND] = strdup("white");
	config->colors[COLOR_MENU_SEPARATOR] = strdup("darkgray");

	config->fonts[FONT_MENU_INPUT] = strdup("sans-serif:pixelsize=14:bold");
	config->fonts[FONT_MENU_ITEM] = strdup("sans-serif:pixelsize=14:bold");

	config->border_width = 1;

	TAILQ_INIT(&config->commands);
	TAILQ_INIT(&config->keybindings);
	TAILQ_INIT(&config->mousebindings);

	config_add_command(config, "terminal", "xterm");

	stream = fopen(path, "r");
	if (stream == NULL) {
		return config;
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
