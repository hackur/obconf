#include "theme.h"
#include "main.h"
#include "gettext.h"

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <zlib.h>
#include <libtar.h>

static gzFile gzf = NULL;

#define gtk_msg(type, args...) \
{                                                                        \
    GtkWidget *msgw;                                                     \
    msgw = gtk_message_dialog_new(GTK_WINDOW(mainwin),                   \
                                  GTK_DIALOG_DESTROY_WITH_PARENT |       \
                                  GTK_DIALOG_MODAL,                      \
                                  type,                                  \
                                  GTK_BUTTONS_OK,                        \
                                  args);                                 \
    gtk_dialog_run(GTK_DIALOG(msgw));                                    \
    gtk_widget_destroy(msgw);                                            \
}

static int gzopen_frontend(const char *path, int oflags, int mode);
static int gzclose_frontend(int nothing);
static ssize_t gzread_frontend(int nothing, void *buf, size_t s);
static ssize_t gzwrite_frontend(int nothing, const void *buf, size_t s);
static gchar *get_theme_dir();
static gboolean change_dir(const gchar *dir);
static gboolean install_theme_to(gchar *theme, gchar *file, gchar *to);
static gchar* name_from_file(const gchar *path);

tartype_t funcs = {
    (openfunc_t) gzopen_frontend,
    (closefunc_t) gzclose_frontend,
    (readfunc_t) gzread_frontend,
    (writefunc_t) gzwrite_frontend
};

gchar* theme_install(gchar *path)
{
    gchar *dest;
    gchar *curdir;
    gchar *name;

    if (!(dest = get_theme_dir()))
        return NULL;

    curdir = g_get_current_dir();
    if (!change_dir(dest)) {
        g_free(curdir);
        return NULL;
    }

    if (!(name = name_from_file(path)))
        return NULL;

    if (install_theme_to(name, path, dest))
        gtk_msg(GTK_MESSAGE_INFO, _("\"%s\" was installed to %s"), name, dest);

    g_free(dest);

    change_dir(curdir);
    g_free(curdir);

    return name;
}

static gchar *get_theme_dir()
{
    gchar *dir;
    gint r;

    dir = g_build_path(G_DIR_SEPARATOR_S, g_get_home_dir(), ".themes", NULL);
    r = mkdir(dir, 0777);
    if (r == -1 && errno != EEXIST) {
        gtk_msg(GTK_MESSAGE_ERROR,
                _("Unable to create directory \"%s\": %s"),
                dir, strerror(errno));
        g_free(dir);
        dir = NULL;
    }

    return dir;
}

static gchar* name_from_file(const gchar *path)
{
    /* decipher the theme name from the file name */
    gchar *fname = g_path_get_basename(path);
    gint len = strlen(fname);
    gchar *name = NULL;

    if (len > 4 &&
        (fname[len-4] == '.' && fname[len-3] == 'o' &&
         fname[len-2] == 'b' && fname[len-1] == 't'))
    {
        fname[len-4] = '\0';
        name = strdup(fname);
        fname[len-4] = '.';
    }

    if (name == NULL)
        gtk_msg(GTK_MESSAGE_ERROR,
                _("Unable to determine the theme's name from \"%s\".  File name should be ThemeName.obt."), fname);

    return name;
}

static gboolean change_dir(const gchar *dir)
{
    if (chdir(dir) == -1) {
        gtk_msg(GTK_MESSAGE_ERROR, _("Unable to move to directory \"%s\": %s"),
                dir, strerror(errno));
        return FALSE;
    }
    return TRUE;
}

static gboolean install_theme_to(gchar *theme, gchar *file, gchar *to)
{
    TAR *t;
    gchar *glob;
    gint r;

    if (tar_open(&t, file, &funcs, 0, O_RDONLY, TAR_GNU) == -1) {
        gtk_msg(GTK_MESSAGE_ERROR,
                _("Unable to open the file \"%s\": %s"),
                file, strerror(errno));
        return FALSE;
    }

    glob = g_strdup_printf("%s/openbox-3/*", theme);
    r = tar_extract_glob(t, glob, to);
    g_free(glob);

    tar_close(t);

    if (r != 0)
        gtk_msg(GTK_MESSAGE_ERROR,
                _("Unable to extract the file \"%s\".\nIt does not appear to be a valid Openbox theme archive (in tar.gz format)."),
                file, strerror(errno));

    return r == 0;
}

static int gzopen_frontend(const char *path, int oflags, int mode)
{
    int fd;

    if ((fd = open(path, oflags, mode)) < 0) return -1;
    if (!(gzf = gzdopen(fd, "rb"))) return -1;
    return 1;
}

static int gzclose_frontend(int nothing)
{
    g_return_val_if_fail(gzf != NULL, 0);
    return gzclose(gzf);
}

static ssize_t gzread_frontend(int nothing, void *buf, size_t s)
{
    return gzread(gzf, buf, s);
}

static ssize_t gzwrite_frontend(int nothing, const void *buf, size_t s)
{
    return gzwrite(gzf, buf, s);
}
