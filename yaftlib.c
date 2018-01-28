/* See LICENSE for licence details. */
#include "yaftlib.h"

/* util.h */
/* error functions */
enum loglevel_t {
	LOG_DEBUG = 0,
	LOG_WARN,
	LOG_ERROR,
	LOG_FATAL,
};

void logging(enum loglevel_t loglevel, char *format, ...)
{
	va_list arg;
	static const char *loglevel2str[] = {
		[LOG_DEBUG] = "DEBUG",
		[LOG_WARN]  = "WARN",
		[LOG_ERROR] = "ERROR",
		[LOG_FATAL] = "FATAL",
	};

	/* debug message is available on verbose mode */
	if ((loglevel == LOG_DEBUG) && (VERBOSE == false))
		return;

	fprintf(stderr, ">>%s<<\t", loglevel2str[loglevel]);

	va_start(arg, format);
	vfprintf(stderr, format, arg);
	va_end(arg);
}

/* wrapper of C functions */
int eopen(const char *path, int flag)
{
	int fd;
	errno = 0;

	if ((fd = open(path, flag)) < 0) {
		logging(LOG_ERROR, "couldn't open \"%s\"\n", path);
		logging(LOG_ERROR, "open: %s\n", strerror(errno));
	}
	return fd;
}

int eclose(int fd)
{
	int ret;
	errno = 0;

	if ((ret = close(fd)) < 0)
		logging(LOG_ERROR, "close: %s\n", strerror(errno));

	return ret;
}

FILE *efopen(const char *path, char *mode)
{
	FILE *fp;
	errno = 0;

	if ((fp = fopen(path, mode)) == NULL) {
		logging(LOG_ERROR, "couldn't open \"%s\"\n", path);
		logging(LOG_ERROR, "fopen: %s\n", strerror(errno));
	}
	return fp;
}

int efclose(FILE *fp)
{
	int ret;
	errno = 0;

	if ((ret = fclose(fp)) < 0)
		logging(LOG_ERROR, "fclose: %s\n", strerror(errno));

	return ret;
}

void *emmap(void *addr, size_t len, int prot, int flag, int fd, off_t offset)
{
	void *fp;
	errno = 0;

	if ((fp = mmap(addr, len, prot, flag, fd, offset)) == MAP_FAILED)
		logging(LOG_ERROR, "mmap: %s\n", strerror(errno));

	return fp;
}

int emunmap(void *ptr, size_t len)
{
	int ret;
	errno = 0;

	if ((ret = munmap(ptr, len)) < 0)
		logging(LOG_ERROR, "munmap: %s\n", strerror(errno));

	return ret;
}

void *ecalloc(size_t nmemb, size_t size)
{
	void *ptr;
	errno = 0;

	if ((ptr = calloc(nmemb, size)) == NULL)
		logging(LOG_ERROR, "calloc: %s\n", strerror(errno));

	return ptr;
}

void *erealloc(void *ptr, size_t size)
{
	void *new;
	errno = 0;

	if ((new = realloc(ptr, size)) == NULL)
		logging(LOG_ERROR, "realloc: %s\n", strerror(errno));

	return new;
}

int eselect(int maxfd, fd_set *readfds, fd_set *writefds, fd_set *errorfds, struct timeval *tv)
{
	int ret;
	errno = 0;

	if ((ret = select(maxfd, readfds, writefds, errorfds, tv)) < 0) {
		if (errno == EINTR)
			return eselect(maxfd, readfds, writefds, errorfds, tv);
		else
			logging(LOG_ERROR, "select: %s\n", strerror(errno));
	}
	return ret;
}

ssize_t ewrite(int fd, const void *buf, size_t size)
{
	ssize_t ret;
	errno = 0;

	if ((ret = write(fd, buf, size)) < 0) {
		if (errno == EINTR) {
			logging(LOG_ERROR, "write: EINTR occurred\n");
			return ewrite(fd, buf, size);
		} else if (errno == EAGAIN || errno == EWOULDBLOCK) {
			logging(LOG_ERROR, "write: EAGAIN or EWOULDBLOCK occurred, sleep %d usec\n", SLEEP_TIME);
			usleep(SLEEP_TIME);
			return ewrite(fd, buf, size);
		} else {
			logging(LOG_ERROR, "write: %s\n", strerror(errno));
			return ret;
		}
	} else if (ret < (ssize_t) size) {
		logging(LOG_ERROR, "data size:%zu write size:%zd\n", size, ret);
		return ewrite(fd, (char *) buf + ret, size - ret);
	}
	return ret;
}

int esigaction(int signo, struct sigaction *act, struct sigaction *oact)
{
	int ret;
	errno = 0;

	if ((ret = sigaction(signo, act, oact)) < 0)
		logging(LOG_ERROR, "sigaction: %s\n", strerror(errno));

	return ret;
}

int etcgetattr(int fd, struct termios *tm)
{
	int ret;
	errno = 0;

	if ((ret = tcgetattr(fd, tm)) < 0)
		logging(LOG_ERROR, "tcgetattr: %s\n", strerror(errno));

	return ret;
}

int etcsetattr(int fd, int action, const struct termios *tm)
{
	int ret;
	errno = 0;

	if ((ret = tcsetattr(fd, action, tm)) < 0)
		logging(LOG_ERROR, "tcgetattr: %s\n", strerror(errno));

	return ret;
}

int eopenpty(int *amaster, int *aslave, char *aname,
	const struct termios *termp, const struct winsize *winsize)
{
	int master;
	char *name = NULL;
	errno = 0;

	if ((master = posix_openpt(O_RDWR | O_NOCTTY)) < 0
		|| grantpt(master) < 0
		|| unlockpt(master) < 0
		|| (name = ptsname(master)) == NULL) {
		logging(LOG_ERROR, "openpty: %s\n", strerror(errno));
		return -1;
	}
	*amaster = master;
	*aslave  = eopen(name, O_RDWR | O_NOCTTY);

	if (aname)
		/* XXX: we don't use the slave's name, do nothing */
		(void) aname;
		//strncpy(aname, name, _POSIX_TTY_NAME_MAX - 1);
		//snprintf(aname, _POSIX_TTY_NAME_MAX, "%s", name);
	if (termp)
		etcsetattr(*aslave, TCSAFLUSH, termp);
	if (winsize)
		ioctl(*aslave, TIOCSWINSZ, winsize);

	return 0;
}

pid_t eforkpty(int *amaster, char *name,
	const struct termios *termp, const struct winsize *winsize)
{
	int master, slave;
	pid_t pid;

	if (eopenpty(&master, &slave, name, termp, winsize) < 0)
		return -1;

	errno = 0;
	pid   = fork();
	if (pid < 0) {
		logging(LOG_ERROR, "fork: %s\n", strerror(errno));
		return pid;
	} else if (pid == 0) { /* child */
		close(master);
		setsid();

		dup2(slave, STDIN_FILENO);
		dup2(slave, STDOUT_FILENO);
		dup2(slave, STDERR_FILENO);

		/* XXX: this ioctl may fail in Mac OS X
			ref http://www.opensource.apple.com/source/Libc/Libc-825.25/util/pty.c?txt */
		if (ioctl(slave, TIOCSCTTY, NULL))
			logging(LOG_WARN, "ioctl: TIOCSCTTY faild\n");
		close(slave);

		return 0;
	}
	/* parent */
	close(slave);
	*amaster = master;

	return pid;
}

int esetenv(const char *name, const char *value, int overwrite)
{
	int ret;
	errno = 0;

	if ((ret = setenv(name, value, overwrite)) < 0)
		logging(LOG_ERROR, "setenv: %s\n", strerror(errno));

	return ret;
}

int eexecvp(const char *file, const char *argv[])
{
	int ret;
	errno = 0;

	if ((ret = execvp(file, (char * const *) argv)) < 0)
		logging(LOG_ERROR, "execvp: %s\n", strerror(errno));

	return ret;
}

int eexecl(const char *path)
{
	int ret;
	errno = 0;

	/* XXX: assume only one argument is given */
	if ((ret = execl(path, path, NULL)) < 0)
		logging(LOG_ERROR, "execl: %s\n", strerror(errno));

	return ret;
}

long estrtol(const char *nptr, char **endptr, int base)
{
	long int ret;
	errno = 0;

	ret = strtol(nptr, endptr, base);
	if (ret == LONG_MIN || ret == LONG_MAX) {
		logging(LOG_ERROR, "strtol: %s\n", strerror(errno));
		return 0;
	}

	return ret;
}

/* parse_arg functions */
void reset_parm(struct parm_t *pt)
{
	pt->argc = 0;
	for (int i = 0; i < MAX_ARGS; i++)
		pt->argv[i] = NULL;
}

void add_parm(struct parm_t *pt, char *cp)
{
	if (pt->argc >= MAX_ARGS)
		return;

	logging(DEBUG, "argv[%d]: %s\n", pt->argc, (cp == NULL) ? "NULL": cp);

	pt->argv[pt->argc] = cp;
	pt->argc++;
}

void parse_arg(char *buf, struct parm_t *pt, int delim, int (is_valid)(int c))
{
	/*
		v..........v d           v.....v d v.....v ... d
		(valid char) (delimiter)
		argv[0]                  argv[1]   argv[2] ...   argv[argc - 1]
	*/
	size_t length;
	char *cp, *vp;

	if (buf == NULL)
		return;

	length = strlen(buf);
	logging(DEBUG, "parse_arg() length:%u\n", (unsigned) length);

	vp = NULL;
	for (size_t i = 0; i < length; i++) {
		cp = buf + i;

		if (vp == NULL && is_valid(*cp))
			vp = cp;

		if (*cp == delim) {
			*cp = '\0';
			add_parm(pt, vp);
			vp = NULL;
		}

		if (i == (length - 1) && (vp != NULL || *cp == '\0'))
			add_parm(pt, vp);
	}

	logging(DEBUG, "argc:%d\n", pt->argc);
}

/* other functions */
int my_ceil(int val, int div)
{
	if (div == 0)
		return 0;
	else
		return (val + div - 1) / div;
}

int dec2num(char *str)
{
	if (str == NULL)
		return 0;

	return (int) estrtol(str, NULL, 10);
}

int hex2num(char *str)
{
	if (str == NULL)
		return 0;

	return (int) estrtol(str, NULL, 16);
}

int sum(struct parm_t *parm)
{
	int sum = 0;

	for (int i = 0; i < parm->argc; i++)
		sum += dec2num(parm->argv[i]);

	return sum;
}

/* terminal.h */
void erase_cell(struct terminal_t *term, int y, int x)
{
	struct cell_t *cellp;

	cellp             = &term->cells[y][x];
	cellp->glyphp     = term->glyph[DEFAULT_CHAR];
	cellp->color_pair = term->color_pair; /* bce */
	cellp->attribute  = ATTR_RESET;
	cellp->width      = HALF;

	term->line_dirty[y] = true;
}

void copy_cell(struct terminal_t *term, int dst_y, int dst_x, int src_y, int src_x)
{
	struct cell_t *dst, *src;

	dst = &term->cells[dst_y][dst_x];
	src = &term->cells[src_y][src_x];

	if (src->width == NEXT_TO_WIDE) {
		return;
	} else if (src->width == WIDE && dst_x == (term->cols - 1)) {
		erase_cell(term, dst_y, dst_x);
	} else {
		*dst = *src;
		if (src->width == WIDE) {
			*(dst + 1) = *src;
			(dst + 1)->width = NEXT_TO_WIDE;
		}
		term->line_dirty[dst_y] = true;
	}
}

int set_cell(struct terminal_t *term, int y, int x, const struct glyph_t *glyphp)
{
	struct cell_t cell, *cellp;
	uint8_t color_tmp;

	cell.glyphp = glyphp;

	cell.color_pair.fg = (term->attribute & attr_mask[ATTR_BOLD] && term->color_pair.fg <= 7) ?
		term->color_pair.fg + BRIGHT_INC: term->color_pair.fg;
	cell.color_pair.bg = (term->attribute & attr_mask[ATTR_BLINK] && term->color_pair.bg <= 7) ?
		term->color_pair.bg + BRIGHT_INC: term->color_pair.bg;

	if (term->attribute & attr_mask[ATTR_REVERSE]) {
		color_tmp          = cell.color_pair.fg;
		cell.color_pair.fg = cell.color_pair.bg;
		cell.color_pair.bg = color_tmp;
	}

	cell.attribute  = term->attribute;
	cell.width      = glyphp->width;

	cellp    = &term->cells[y][x];
	*cellp   = cell;
	term->line_dirty[y] = true;

	if (cell.width == WIDE && x + 1 < term->cols) {
		cellp        = &term->cells[y][x + 1];
		*cellp       = cell;
		cellp->width = NEXT_TO_WIDE;
		return WIDE;
	}

	if (cell.width == HALF /* isolated NEXT_TO_WIDE cell */
		&& x + 1 < term->cols
		&& term->cells[y][x + 1].width == NEXT_TO_WIDE) {
		erase_cell(term, y, x + 1);
	}
	return HALF;
}

static inline void swap_lines(struct terminal_t *term, int i, int j)
{
	struct cell_t *tmp;

	tmp            = term->cells[i];
	term->cells[i] = term->cells[j];
	term->cells[j] = tmp;
}

void scroll_window(struct terminal_t *term, int from, int to, int offset)
{
	int abs_offset, lines;

	if (offset == 0 || from >= to)
		return;

	logging(DEBUG, "scroll from:%d to:%d offset:%d\n", from, to, offset);

	for (int y = from; y <= to; y++)
		term->line_dirty[y] = true;

	abs_offset = abs(offset);
	lines      = (to - from + 1) - abs_offset;

	if (offset > 0) { /* scroll down */
		for (int y = from; y < from + lines; y++)
			swap_lines(term, y, y + offset);
		for (int y = (to - offset + 1); y <= to; y++)
			for (int x = 0; x < term->cols; x++)
				erase_cell(term, y, x);
	}
	else {            /* scroll up */
		for (int y = to; y >= from + abs_offset; y--)
			swap_lines(term, y, y - abs_offset);
		for (int y = from; y < from + abs_offset; y++)
			for (int x = 0; x < term->cols; x++)
				erase_cell(term, y, x);
	}
}

/* relative movement: cause scrolling */
void move_cursor(struct terminal_t *term, int y_offset, int x_offset)
{
	int x, y, top, bottom;

	x = term->cursor.x + x_offset;
	y = term->cursor.y + y_offset;

	top    = term->scroll.top;
	bottom = term->scroll.bottom;

	if (x < 0) {
		x = 0;
	} else if (x >= term->cols) {
		if (term->mode & MODE_AMRIGHT)
			term->wrap_occured = true;
		x = term->cols - 1;
	}
	term->cursor.x = x;

	y = (y < 0) ? 0:
		(y >= term->lines) ? term->lines - 1: y;

	if (term->cursor.y == top && y_offset < 0) {
		y = top;
		scroll_window(term, top, bottom, y_offset);
	} else if (term->cursor.y == bottom && y_offset > 0) {
		y = bottom;
		scroll_window(term, top, bottom, y_offset);
	}
	term->cursor.y = y;
}

/* absolute movement: never scroll */
void set_cursor(struct terminal_t *term, int y, int x)
{
	int top, bottom;

	if (term->mode & MODE_ORIGIN) {
		top    = term->scroll.top;
		bottom = term->scroll.bottom;
		y += term->scroll.top;
	} else {
		top = 0;
		bottom = term->lines - 1;
	}

	x = (x < 0) ? 0: (x >= term->cols) ? term->cols - 1: x;
	y = (y < top) ? top: (y > bottom) ? bottom: y;

	term->cursor.x = x;
	term->cursor.y = y;
	term->wrap_occured = false;
}

void add_char(struct terminal_t *term, uint32_t code)
{
	int width;
	const struct glyph_t *glyphp;

	logging(DEBUG, "add_char: U+%.4X\n", code);

	width = wcwidth(code);

	if (width <= 0)                                /* zero width: not support comibining character */
		return;
	else if (code >= UCS2_CHARS                    /* yaft support only UCS2 */
		|| term->glyph[code] == NULL           /* missing glyph */
		|| term->glyph[code]->width != width)  /* width unmatch */
		glyphp = (width == 1) ? term->glyph[SUBSTITUTE_HALF]: term->glyph[SUBSTITUTE_WIDE];
	else
		glyphp = term->glyph[code];

	if ((term->wrap_occured && term->cursor.x == term->cols - 1) /* folding */
		|| (glyphp->width == WIDE && term->cursor.x == term->cols - 1)) {
		set_cursor(term, term->cursor.y, 0);
		move_cursor(term, 1, 0);
	}
	term->wrap_occured = false;

	move_cursor(term, 0, set_cell(term, term->cursor.y, term->cursor.x, glyphp));
}

void reset_esc(struct terminal_t *term)
{
	logging(DEBUG, "*esc reset*\n");

	term->esc.bp    = term->esc.buf;
	term->esc.state = STATE_RESET;
}

bool push_esc(struct terminal_t *term, uint8_t ch)
{
	long offset;

	if ((term->esc.bp - term->esc.buf) >= term->esc.size) { /* buffer limit */
		logging(DEBUG, "escape sequence length >= %d, term.esc.buf reallocated\n", term->esc.size);
		offset = term->esc.bp - term->esc.buf;
		term->esc.buf = erealloc(term->esc.buf, term->esc.size * 2);
		term->esc.bp  = term->esc.buf + offset;
		term->esc.size *= 2;
	}

	/* ref: http://www.vt100.net/docs/vt102-ug/appendixd.html */
	*term->esc.bp++ = ch;
	if (term->esc.state == STATE_ESC) {
		/* format:
			ESC  I.......I F
				 ' '  '/'  '0'  '~'
			0x1B 0x20-0x2F 0x30-0x7E
		*/
		if ('0' <= ch && ch <= '~')        /* final char */
			return true;
		else if (SPACE <= ch && ch <= '/') /* intermediate char */
			return false;
	} else if (term->esc.state == STATE_CSI) {
		/* format:
			CSI       P.......P I.......I F
			ESC  '['  '0'  '?'  ' '  '/'  '@'  '~'
			0x1B 0x5B 0x30-0x3F 0x20-0x2F 0x40-0x7E
		*/
		if ('@' <= ch && ch <= '~')
			return true;
		else if (SPACE <= ch && ch <= '?')
			return false;
	} else {
		/* format:
			OSC       I.....I F
			ESC  ']'          BEL  or ESC  '\'
			0x1B 0x5D unknown 0x07 or 0x1B 0x5C
			DCS       I....I  F
			ESC  'P'          BEL  or ESC  '\'
			0x1B 0x50 unknown 0x07 or 0x1B 0x5C
		*/
		if (ch == BEL || (ch == BACKSLASH
			&& (term->esc.bp - term->esc.buf) >= 2 && *(term->esc.bp - 2) == ESC))
			return true;
		else if ((ch == ESC || ch == CR || ch == LF || ch == BS || ch == HT)
			|| (SPACE <= ch && ch <= '~'))
			return false;
	}

	/* invalid sequence */
	reset_esc(term);
	return false;
}

void reset_charset(struct terminal_t *term)
{
	term->charset.code = term->charset.count = term->charset.following_byte = 0;
	term->charset.is_valid = true;
}

void reset(struct terminal_t *term)
{
	term->mode  = MODE_RESET;
	term->mode |= (MODE_CURSOR | MODE_AMRIGHT);
	term->wrap_occured = false;

	term->scroll.top    = 0;
	term->scroll.bottom = term->lines - 1;

	term->cursor.x = term->cursor.y = 0;

	term->state.mode      = term->mode;
	term->state.cursor    = term->cursor;
	term->state.attribute = ATTR_RESET;

	term->color_pair.fg = DEFAULT_FG;
	term->color_pair.bg = DEFAULT_BG;

	term->attribute = ATTR_RESET;

	for (int line = 0; line < term->lines; line++) {
		for (int col = 0; col < term->cols; col++) {
			erase_cell(term, line, col);
			if ((col % TABSTOP) == 0)
				term->tabstop[col] = true;
			else
				term->tabstop[col] = false;
		}
		term->line_dirty[line] = true;
	}

	reset_esc(term);
	reset_charset(term);
}

void redraw(struct terminal_t *term)
{
	for (int i = 0; i < term->lines; i++)
		term->line_dirty[i] = true;
}

void term_die(struct terminal_t *term)
{
	free(term->line_dirty);
	free(term->tabstop);
	free(term->esc.buf);

	for (int i = 0; i < term->lines; i++)
		free(term->cells[i]);
	free(term->cells);
}

bool term_init(struct terminal_t *term, int width, int height)
{
	extern const uint32_t color_list[COLORS]; /* global */

	term->width  = width;
	term->height = height;

	term->cols  = term->width / CELL_WIDTH;
	term->lines = term->height / CELL_HEIGHT;

	term->esc.size = ESCSEQ_SIZE;

	logging(DEBUG, "terminal cols:%d lines:%d\n", term->cols, term->lines);

	/* allocate memory */
	term->line_dirty   = (bool *) ecalloc(term->lines, sizeof(bool));
	term->tabstop      = (bool *) ecalloc(term->cols, sizeof(bool));
	term->esc.buf      = (char *) ecalloc(1, term->esc.size);

	term->cells        = (struct cell_t **) ecalloc(term->lines, sizeof(struct cell_t *));
	for (int i = 0; i < term->lines; i++)
		term->cells[i] = (struct cell_t *) ecalloc(term->cols, sizeof(struct cell_t));

	if (!term->line_dirty || !term->tabstop
		|| !term->cells || !term->esc.buf) {
		term_die(term);
		return false;
	}

	/* initialize palette */
	for (int i = 0; i < COLORS; i++)
		term->virtual_palette[i] = color_list[i];
	term->palette_modified = false;

	/* initialize glyph map */
	for (uint32_t code = 0; code < UCS2_CHARS; code++)
		term->glyph[code] = NULL;

	for (uint32_t gi = 0; gi < sizeof(glyphs) / sizeof(struct glyph_t); gi++)
		term->glyph[glyphs[gi].code] = &glyphs[gi];

	if (!term->glyph[DEFAULT_CHAR]
		|| !term->glyph[SUBSTITUTE_HALF]
		|| !term->glyph[SUBSTITUTE_WIDE]) {
		logging(LOG_ERROR, "couldn't find essential glyph:\
			DEFAULT_CHAR(U+%.4X):%p SUBSTITUTE_HALF(U+%.4X):%p SUBSTITUTE_WIDE(U+%.4X):%p\n",
			DEFAULT_CHAR, term->glyph[DEFAULT_CHAR],
			SUBSTITUTE_HALF, term->glyph[SUBSTITUTE_HALF],
			SUBSTITUTE_WIDE, term->glyph[SUBSTITUTE_WIDE]);
		return false;
	}

	/* reset terminal */
	reset(term);

	return true;
}

/* esc.h */
/* function for control character */
void bs(struct terminal_t *term)
{
	if (term->mode & MODE_VWBS
		&& term->cursor.x - 1 >= 0
		&& term->cells[term->cursor.y][term->cursor.x - 1].width == NEXT_TO_WIDE)
		move_cursor(term, 0, -2);
	else
		move_cursor(term, 0, -1);
}

void tab(struct terminal_t *term)
{
	int i;

	for (i = term->cursor.x + 1; i < term->cols; i++) {
		if (term->tabstop[i]) {
			set_cursor(term, term->cursor.y, i);
			return;
		}
	}
	set_cursor(term, term->cursor.y, term->cols - 1);
}

void newline(struct terminal_t *term)
{
	move_cursor(term, 1, 0);
}

void carriage_return(struct terminal_t *term)
{
	set_cursor(term, term->cursor.y, 0);
}

void enter_esc(struct terminal_t *term)
{
	term->esc.state = STATE_ESC;
}

/* function for escape sequence */
void save_state(struct terminal_t *term)
{
	term->state.mode = term->mode & MODE_ORIGIN;
	term->state.cursor = term->cursor;
	term->state.attribute = term->attribute;
}

void restore_state(struct terminal_t *term)
{
	/* restore state */
	if (term->state.mode & MODE_ORIGIN)
		term->mode |= MODE_ORIGIN;
	else
		term->mode &= ~MODE_ORIGIN;
	term->cursor    = term->state.cursor;
	term->attribute = term->state.attribute;
}

void crnl(struct terminal_t *term)
{
	carriage_return(term);
	newline(term);
}

void set_tabstop(struct terminal_t *term)
{
	term->tabstop[term->cursor.x] = true;
}

void reverse_nl(struct terminal_t *term)
{
	move_cursor(term, -1, 0);
}

void identify(struct terminal_t *term)
{
	ewrite(term->fd, "\033[?6c", 5); /* "I am a VT102" */
}

void enter_csi(struct terminal_t *term)
{
	term->esc.state = STATE_CSI;
}

void enter_osc(struct terminal_t *term)
{
	term->esc.state = STATE_OSC;
}

void enter_dcs(struct terminal_t *term)
{
	term->esc.state = STATE_DCS;
}

void ris(struct terminal_t *term)
{
	reset(term);
}

/* csi.h */
/* function for csi sequence */
void insert_blank(struct terminal_t *term, struct parm_t *parm)
{
	int i, num = sum(parm);

	if (num <= 0)
		num = 1;

	for (i = term->cols - 1; term->cursor.x <= i; i--) {
		if (term->cursor.x <= (i - num))
			copy_cell(term, term->cursor.y, i, term->cursor.y, i - num);
		else
			erase_cell(term, term->cursor.y, i);
	}
}

void curs_up(struct terminal_t *term, struct parm_t *parm)
{
	int num = sum(parm);

	if (num <= 0)
		num = 1;

	move_cursor(term, -num, 0);
}

void curs_down(struct terminal_t *term, struct parm_t *parm)
{
	int num = sum(parm);

	if (num <= 0)
		num = 1;

	move_cursor(term, num, 0);
}

void curs_forward(struct terminal_t *term, struct parm_t *parm)
{
	int num = sum(parm);

	if (num <= 0)
		num = 1;

	move_cursor(term, 0, num);
}

void curs_back(struct terminal_t *term, struct parm_t *parm)
{
	int num = sum(parm);

	if (num <= 0)
		num = 1;

	move_cursor(term, 0, -num);
}

void curs_nl(struct terminal_t *term, struct parm_t *parm)
{
	int num = sum(parm);

	if (num <= 0)
		num = 1;

	move_cursor(term, num, 0);
	carriage_return(term);
}

void curs_pl(struct terminal_t *term, struct parm_t *parm)
{
	int num = sum(parm);

	if (num <= 0)
		num = 1;

	move_cursor(term, -num, 0);
	carriage_return(term);
}

void curs_col(struct terminal_t *term, struct parm_t *parm)
{
	int num;

	num = (parm->argc <= 0) ? 0: dec2num(parm->argv[parm->argc - 1]) - 1;
	set_cursor(term, term->cursor.y, num);
}

void curs_pos(struct terminal_t *term, struct parm_t *parm)
{
	int line, col;

	if (parm->argc <= 0) {
		line = col = 0;
	} else if (parm->argc == 2) {
		line = dec2num(parm->argv[0]) - 1;
		col  = dec2num(parm->argv[1]) - 1;
	} else {
		return;
	}

	if (line < 0)
		line = 0;
	if (col < 0)
		col = 0;

	set_cursor(term, line, col);
}

void curs_line(struct terminal_t *term, struct parm_t *parm)
{
	int num;

	num = (parm->argc <= 0) ? 0: dec2num(parm->argv[parm->argc - 1]) - 1;
	set_cursor(term, num, term->cursor.x);
}

void erase_display(struct terminal_t *term, struct parm_t *parm)
{
	int i, j, mode;

	mode = (parm->argc <= 0) ? 0: dec2num(parm->argv[parm->argc - 1]);

	if (mode < 0 || 2 < mode)
		return;

	if (mode == 0) {
		for (i = term->cursor.y; i < term->lines; i++)
			for (j = 0; j < term->cols; j++)
				if (i > term->cursor.y || (i == term->cursor.y && j >= term->cursor.x))
					erase_cell(term, i, j);
	} else if (mode == 1) {
		for (i = 0; i <= term->cursor.y; i++)
			for (j = 0; j < term->cols; j++)
				if (i < term->cursor.y || (i == term->cursor.y && j <= term->cursor.x))
					erase_cell(term, i, j);
	} else if (mode == 2) {
		for (i = 0; i < term->lines; i++)
			for (j = 0; j < term->cols; j++)
				erase_cell(term, i, j);
	}
}

void erase_line(struct terminal_t *term, struct parm_t *parm)
{
	int i, mode;

	mode = (parm->argc <= 0) ? 0: dec2num(parm->argv[parm->argc - 1]);

	if (mode < 0 || 2 < mode)
		return;

	if (mode == 0) {
		for (i = term->cursor.x; i < term->cols; i++)
			erase_cell(term, term->cursor.y, i);
	} else if (mode == 1) {
		for (i = 0; i <= term->cursor.x; i++)
			erase_cell(term, term->cursor.y, i);
	} else if (mode == 2) {
		for (i = 0; i < term->cols; i++)
			erase_cell(term, term->cursor.y, i);
	}
}

void insert_line(struct terminal_t *term, struct parm_t *parm)
{
	int num = sum(parm);

	if (term->mode & MODE_ORIGIN) {
		if (term->cursor.y < term->scroll.top
			|| term->cursor.y > term->scroll.bottom)
			return;
	}

	if (num <= 0)
		num = 1;

	scroll_window(term, term->cursor.y, term->scroll.bottom, -num);
}

void delete_line(struct terminal_t *term, struct parm_t *parm)
{
	int num = sum(parm);

	if (term->mode & MODE_ORIGIN) {
		if (term->cursor.y < term->scroll.top
			|| term->cursor.y > term->scroll.bottom)
			return;
	}

	if (num <= 0)
		num = 1;

	scroll_window(term, term->cursor.y, term->scroll.bottom, num);
}

void delete_char(struct terminal_t *term, struct parm_t *parm)
{
	int i, num = sum(parm);

	if (num <= 0)
		num = 1;

	for (i = term->cursor.x; i < term->cols; i++) {
		if ((i + num) < term->cols)
			copy_cell(term, term->cursor.y, i, term->cursor.y, i + num);
		else
			erase_cell(term, term->cursor.y, i);
	}
}

void erase_char(struct terminal_t *term, struct parm_t *parm)
{
	int i, num = sum(parm);

	if (num <= 0)
		num = 1;
	else if (num + term->cursor.x > term->cols)
		num = term->cols - term->cursor.x;

	for (i = term->cursor.x; i < term->cursor.x + num; i++)
		erase_cell(term, term->cursor.y, i);
}

uint8_t rgb2index(uint8_t r, uint8_t g, uint8_t b)
{
	/* SGR: Set Graphic Rendition (special case)
	 * 	special color selection (from 256color index or as a 24bit color value)
	 *
	 *	ESC [ 38 ; 5 ; Ps m
	 *	ESC [ 48 ; 5 ; Ps m
	 *		select foreground/background color from 256 color index
	 *
	 *	ESC [ 38 ; 2 ; r ; g ; b m
	 *	ESC [ 48 ; 2 ; r ; g ; b m
	 *		select foreground/background color as a 24bit color value
	 *
	 * 	according to ITU T.416 (https://www.itu.int/rec/T-REC-T.416/en)
	 * 	this format is valid (but most of terminals don't support)
	 *	ESC [ 38 : 5 : Ps m
	 *	ESC [ 48 : 5 : Ps m
	 *
	 *	ESC [ 48 : 2 : r : g : b m
	 *	ESC [ 38 : 2 : r : g : b m
	 */
	int padding;
	uint8_t index;

	/* XXX: this calculation fails if color palette modified by OSC 4 */
	/* 256 color palette is defined in color.h */

	if (r == g && g == b) {
		/*
			search from grayscale
				index: 232 - 255
				value: 0x080808 - 0xEEEEEE
				inc  : 0x0A
		*/
		padding = (r - 0x08) / 0x0A;

		if (padding >= 24)
			/* index 231 (0xFFFFFF) is the last color of 6x6x6 cube */
			index = 231;
		else if (padding <= 0)
			index = 232;
		else
			index = 232 + padding;

	} else {
		/*
			search from 6x6x6 color cube
				index: 16 - 231
				each rgb takes 6 values: {0x00, 0x5F, 0x87, 0xAF, 0xD7, 0xFF}
		*/
		const uint8_t values[] = {0x0, 0x5F, 0x87, 0xAF, 0xD7, 0xFF};
		const uint8_t rgb[] = {r, g, b};
		uint8_t closest_index[3];
		int small, big;

		for (int c = 0; c < 3; c++) {
			for (int i = 0; i < 5; i++) {
				if (values[i] <= rgb[c] && rgb[c] <= values[i + 1]) {
					small = abs(rgb[c] - values[i]);
					big   = abs(rgb[c] - values[i + 1]);
					if (small < big)
						closest_index[c] = i;
					else
						closest_index[c] = i + 1;
				}
			}
		}
		index = 16 + closest_index[0] * 36 + closest_index[1] * 6 + closest_index[2];
	}

	return index;
}

void set_attr(struct terminal_t *term, struct parm_t *parm)
{
	/* SGR: Set Graphic Rendition
	 * 	ESC [ Pm m
	 * 	Pm:
	 *		0: reset all attribute
	 *
	 *		1: brighten foreground color (only work for color index 0-7)
	 *		4: underline
	 *		5: brighten background color (only work for color index 0-7)
	 *		7: reverse (swap foreground/background color)
	 *
	 *		21: reset brighten foreground color
	 *		24: reset underline
	 *		25: reset brighten background color
	 *		27: reset reverse
	 *
	 *		30-37: select foreground color (color index 0-7)
	 *		38: special foreground color selection: implemented in select_color_value()
	 *
	 *		40-47: select background color (color index 0-7)
	 *		48: special foreground color selection: implemented in select_color_value()
	 */
	int i, num;

	if (parm->argc <= 0) {
		term->attribute     = ATTR_RESET;
		term->color_pair.fg = DEFAULT_FG;
		term->color_pair.bg = DEFAULT_BG;
		return;
	}

	for (i = 0; i < parm->argc; i++) {
		num = dec2num(parm->argv[i]);
		//logging(DEBUG, "argc:%d num:%d\n", parm->argc, num);

		if (num == 0) {                        /* reset all attribute and color */
			term->attribute = ATTR_RESET;
			term->color_pair.fg = DEFAULT_FG;
			term->color_pair.bg = DEFAULT_BG;
		} else if (1 <= num && num <= 7) {     /* set attribute */
			term->attribute |= attr_mask[num];
		} else if (21 <= num && num <= 27) {   /* reset attribute */
			term->attribute &= ~attr_mask[num - 20];
		} else if (30 <= num && num <= 37) {   /* set foreground */
			term->color_pair.fg = (num - 30);
		} else if (num == 38) {                /* special foreground color selection */
			/* select foreground color from 256 color index */
			if ((i + 2) < parm->argc && dec2num(parm->argv[i + 1]) == 5) {
				term->color_pair.fg = dec2num(parm->argv[i + 2]);
				i += 2;
			/* select foreground color from specified rgb color */
			} else if ((i + 4) < parm->argc && dec2num(parm->argv[i + 1]) == 2) {
				term->color_pair.fg = rgb2index(dec2num(parm->argv[i + 2]),
					dec2num(parm->argv[i + 3]), dec2num(parm->argv[i + 4]));
				i += 4;
			}
		} else if (num == 39) {                /* reset foreground */
			term->color_pair.fg = DEFAULT_FG;
		} else if (40 <= num && num <= 47) {   /* set background */
			term->color_pair.bg = (num - 40);
		} else if (num == 48) {                /* special background  color selection */
			/* select background color from 256 color index */
			if ((i + 2) < parm->argc && dec2num(parm->argv[i + 1]) == 5) {
				term->color_pair.bg = dec2num(parm->argv[i + 2]);
				i += 2;
			/* select background color from specified rgb color */
			} else if ((i + 4) < parm->argc && dec2num(parm->argv[i + 1]) == 2) {
				term->color_pair.bg = rgb2index(dec2num(parm->argv[i + 2]),
					dec2num(parm->argv[i + 3]), dec2num(parm->argv[i + 4]));
				i += 4;
			}
		} else if (num == 49) {                /* reset background */
			term->color_pair.bg = DEFAULT_BG;
		} else if (90 <= num && num <= 97) {   /* set bright foreground */
			term->color_pair.fg = (num - 90) + BRIGHT_INC;
		} else if (100 <= num && num <= 107) { /* set bright background */
			term->color_pair.bg = (num - 100) + BRIGHT_INC;
		}
	}
}

void status_report(struct terminal_t *term, struct parm_t *parm)
{
	int i, num;
	char buf[BUFSIZE];

	for (i = 0; i < parm->argc; i++) {
		num = dec2num(parm->argv[i]);
		if (num == 5) {         /* terminal response: ready */
			ewrite(term->fd, "\033[0n", 4);
		} else if (num == 6) {  /* cursor position report */
			snprintf(buf, BUFSIZE, "\033[%d;%dR", term->cursor.y + 1, term->cursor.x + 1);
			ewrite(term->fd, buf, strlen(buf));
		} else if (num == 15) { /* terminal response: printer not connected */
			ewrite(term->fd, "\033[?13n", 6);
		}
	}
}

void device_attribute(struct terminal_t *term, struct parm_t *parm)
{
	/* TODO: refer VT525 DA */
	(void) parm;
	ewrite(term->fd, "\033[?6c", 5); /* "I am a VT102" */
}

void set_mode(struct terminal_t *term, struct parm_t *parm)
{
	int i, mode;

	for (i = 0; i < parm->argc; i++) {
		mode = dec2num(parm->argv[i]);
		if (*(term->esc.buf + 1) != '?')
			continue; /* not supported */

		if (mode == 6) { /* private mode */
			term->mode |= MODE_ORIGIN;
			set_cursor(term, 0, 0);
		} else if (mode == 7) {
			term->mode |= MODE_AMRIGHT;
		} else if (mode == 25) {
			term->mode |= MODE_CURSOR;
		} else if (mode == 8901) {
			term->mode |= MODE_VWBS;
		}
	}

}

void reset_mode(struct terminal_t *term, struct parm_t *parm)
{
	int i, mode;

	for (i = 0; i < parm->argc; i++) {
		mode = dec2num(parm->argv[i]);
		if (*(term->esc.buf + 1) != '?')
			continue; /* not supported */

		if (mode == 6) { /* private mode */
			term->mode &= ~MODE_ORIGIN;
			set_cursor(term, 0, 0);
		} else if (mode == 7) {
			term->mode &= ~MODE_AMRIGHT;
			term->wrap_occured = false;
		} else if (mode == 25) {
			term->mode &= ~MODE_CURSOR;
		} else if (mode == 8901) {
			term->mode &= ~MODE_VWBS;
		}
	}

}

void set_margin(struct terminal_t *term, struct parm_t *parm)
{
	int top, bottom;

	if (parm->argc <= 0) {        /* CSI r */
		top    = 0;
		bottom = term->lines - 1;
	} else if (parm->argc == 2) { /* CSI ; r -> use default value */
		top    = (parm->argv[0] == NULL) ? 0: dec2num(parm->argv[0]) - 1;
		bottom = (parm->argv[1] == NULL) ? term->lines - 1: dec2num(parm->argv[1]) - 1;
	} else {
		return;
	}

	if (top < 0 || top >= term->lines)
		top = 0;
	if (bottom < 0 || bottom >= term->lines)
		bottom = term->lines - 1;

	if (top >= bottom)
		return;

	term->scroll.top = top;
	term->scroll.bottom = bottom;

	set_cursor(term, 0, 0); /* move cursor to home */
}

void clear_tabstop(struct terminal_t *term, struct parm_t *parm)
{
	int i, j, num;

	if (parm->argc <= 0) {
		term->tabstop[term->cursor.x] = false;
	} else {
		for (i = 0; i < parm->argc; i++) {
			num = dec2num(parm->argv[i]);
			if (num == 0) {
				term->tabstop[term->cursor.x] = false;
			} else if (num == 3) {
				for (j = 0; j < term->cols; j++)
					term->tabstop[j] = false;
				return;
			}
		}
	}
}

/* parse.h */
void (*ctrl_func[CTRL_CHARS])(struct terminal_t *term) = {
	[BS]  = bs,
	[HT]  = tab,
	[LF]  = newline,
	[VT]  = newline,
	[FF]  = newline,
	[CR]  = carriage_return,
	[ESC] = enter_esc,
};

void (*esc_func[ESC_CHARS])(struct terminal_t *term) = {
	['7'] = save_state,
	['8'] = restore_state,
	['D'] = newline,
	['E'] = crnl,
	['H'] = set_tabstop,
	['M'] = reverse_nl,
	['P'] = enter_dcs,
	['Z'] = identify,
	['['] = enter_csi,
	[']'] = enter_osc,
	['c'] = ris,
};

void (*csi_func[ESC_CHARS])(struct terminal_t *term, struct parm_t *) = {
	['@'] = insert_blank,
	['A'] = curs_up,
	['B'] = curs_down,
	['C'] = curs_forward,
	['D'] = curs_back,
	['E'] = curs_nl,
	['F'] = curs_pl,
	['G'] = curs_col,
	['H'] = curs_pos,
	['J'] = erase_display,
	['K'] = erase_line,
	['L'] = insert_line,
	['M'] = delete_line,
	['P'] = delete_char,
	['X'] = erase_char,
	['a'] = curs_forward,
	['c'] = device_attribute,
	['d'] = curs_line,
	['e'] = curs_down,
	['f'] = curs_pos,
	['g'] = clear_tabstop,
	['h'] = set_mode,
	['l'] = reset_mode,
	['m'] = set_attr,
	['n'] = status_report,
	['r'] = set_margin,
	/* XXX: not implemented because these sequences conflict DECSLRM/DECSHTS
	['s'] = sco_save_state,
	['u'] = sco_restore_state,
	*/
	['`'] = curs_col,
};

/* ctr char/esc sequence/charset function */
void control_character(struct terminal_t *term, uint8_t ch)
{
	static const char *ctrl_char[] = {
		"NUL", "SOH", "STX", "ETX", "EOT", "ENQ", "ACK", "BEL",
		"BS ", "HT ", "LF ", "VT ", "FF ", "CR ", "SO ", "SI ",
		"DLE", "DC1", "DC2", "DC3", "DC4", "NAK", "SYN", "ETB",
		"CAN", "EM ", "SUB", "ESC", "FS ", "GS ", "RS ", "US ",
	};

	*term->esc.bp = '\0';

	logging(DEBUG, "ctl: %s\n", ctrl_char[ch]);

	if (ctrl_func[ch])
		ctrl_func[ch](term);
}

void esc_sequence(struct terminal_t *term, uint8_t ch)
{
	*term->esc.bp = '\0';

	logging(DEBUG, "esc: ESC %s\n", term->esc.buf);

	if (strlen(term->esc.buf) == 1 && esc_func[ch])
		esc_func[ch](term);

	/* not reset if csi/osc/dcs seqence */
	if (ch == '[' || ch == ']' || ch == 'P')
		return;

	reset_esc(term);
}

void csi_sequence(struct terminal_t *term, uint8_t ch)
{
	struct parm_t parm;

	*(term->esc.bp - 1) = '\0'; /* omit final character */

	logging(DEBUG, "csi: CSI %s\n", term->esc.buf + 1);

	reset_parm(&parm);
	parse_arg(term->esc.buf + 1, &parm, ';', isdigit); /* skip '[' */

	if (csi_func[ch])
		csi_func[ch](term, &parm);

	reset_esc(term);
}

void osc_sequence(struct terminal_t *term, uint8_t ch)
{
  (void) ch;
	reset_esc(term);
}

void dcs_sequence(struct terminal_t *term, uint8_t ch)
{
  (void) ch;
	reset_esc(term);
}

void utf8_charset(struct terminal_t *term, uint8_t ch)
{
	if (0x80 <= ch && ch <= 0xBF) {
		/* check illegal UTF-8 sequence
			* ? byte sequence: first byte must be between 0xC2 ~ 0xFD
			* 2 byte sequence: first byte must be between 0xC2 ~ 0xDF
			* 3 byte sequence: second byte following 0xE0 must be between 0xA0 ~ 0xBF
			* 4 byte sequence: second byte following 0xF0 must be between 0x90 ~ 0xBF
			* 5 byte sequence: second byte following 0xF8 must be between 0x88 ~ 0xBF
			* 6 byte sequence: second byte following 0xFC must be between 0x84 ~ 0xBF
		*/
		if ((term->charset.following_byte == 0)
			|| (term->charset.following_byte == 1 && term->charset.count == 0 && term->charset.code <= 1)
			|| (term->charset.following_byte == 2 && term->charset.count == 0 && term->charset.code == 0 && ch < 0xA0)
			|| (term->charset.following_byte == 3 && term->charset.count == 0 && term->charset.code == 0 && ch < 0x90)
			|| (term->charset.following_byte == 4 && term->charset.count == 0 && term->charset.code == 0 && ch < 0x88)
			|| (term->charset.following_byte == 5 && term->charset.count == 0 && term->charset.code == 0 && ch < 0x84))
			term->charset.is_valid = false;

		term->charset.code <<= 6;
		term->charset.code += ch & 0x3F;
		term->charset.count++;
	} else if (0xC0 <= ch && ch <= 0xDF) {
		term->charset.code = ch & 0x1F;
		term->charset.following_byte = 1;
		term->charset.count = 0;
		return;
	} else if (0xE0 <= ch && ch <= 0xEF) {
		term->charset.code = ch & 0x0F;
		term->charset.following_byte = 2;
		term->charset.count = 0;
		return;
	} else if (0xF0 <= ch && ch <= 0xF7) {
		term->charset.code = ch & 0x07;
		term->charset.following_byte = 3;
		term->charset.count = 0;
		return;
	} else if (0xF8 <= ch && ch <= 0xFB) {
		term->charset.code = ch & 0x03;
		term->charset.following_byte = 4;
		term->charset.count = 0;
		return;
	} else if (0xFC <= ch && ch <= 0xFD) {
		term->charset.code = ch & 0x01;
		term->charset.following_byte = 5;
		term->charset.count = 0;
		return;
	} else { /* 0xFE - 0xFF: not used in UTF-8 */
		add_char(term, REPLACEMENT_CHAR);
		reset_charset(term);
		return;
	}

	if (term->charset.count >= term->charset.following_byte) {
		/*	illegal code point (ref: http://www.unicode.org/reports/tr27/tr27-4.html)
			0xD800   ~ 0xDFFF : surrogate pair
			0xFDD0   ~ 0xFDEF : noncharacter
			0xnFFFE  ~ 0xnFFFF: noncharacter (n: 0x00 ~ 0x10)
			0x110000 ~        : invalid (unicode U+0000 ~ U+10FFFF)
		*/
		if (!term->charset.is_valid
			|| (0xD800 <= term->charset.code && term->charset.code <= 0xDFFF)
			|| (0xFDD0 <= term->charset.code && term->charset.code <= 0xFDEF)
			|| ((term->charset.code & 0xFFFF) == 0xFFFE || (term->charset.code & 0xFFFF) == 0xFFFF)
			|| (term->charset.code > 0x10FFFF))
			add_char(term, REPLACEMENT_CHAR);
		else
			add_char(term, term->charset.code);

		reset_charset(term);
	}
}

void parse(struct terminal_t *term, uint8_t *buf, int size)
{
	/*
		CTRL CHARS      : 0x00 ~ 0x1F
		ASCII(printable): 0x20 ~ 0x7E
		CTRL CHARS(DEL) : 0x7F
		UTF-8           : 0x80 ~ 0xFF
	*/
	uint8_t ch;

	for (int i = 0; i < size; i++) {
		ch = buf[i];
		if (term->esc.state == STATE_RESET) {
			/* interrupted by illegal byte */
			if (term->charset.following_byte > 0 && (ch < 0x80 || ch > 0xBF)) {
				add_char(term, REPLACEMENT_CHAR);
				reset_charset(term);
			}

			if (ch <= 0x1F)
				control_character(term, ch);
			else if (ch <= 0x7F)
				add_char(term, ch);
			else
				utf8_charset(term, ch);
		} else if (term->esc.state == STATE_ESC) {
			if (push_esc(term, ch))
				esc_sequence(term, ch);
		} else if (term->esc.state == STATE_CSI) {
			if (push_esc(term, ch))
				csi_sequence(term, ch);
		} else if (term->esc.state == STATE_OSC) {
			if (push_esc(term, ch))
				osc_sequence(term, ch);
		} else if (term->esc.state == STATE_DCS) {
			if (push_esc(term, ch))
				dcs_sequence(term, ch);
		}
	}
}
