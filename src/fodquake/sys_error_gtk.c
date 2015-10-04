/*
Copyright (C) 2009, 2011 Mark Olsen
Copyright (C) 2009 Jürgen Legler

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#define G_DISABLE_CAST_CHECKS

#include <dlfcn.h>
#include <unistd.h>
#include <sys/wait.h>
#include <gtk/gtk.h>

#include <string.h>
#include <stdlib.h>

#include "sys_error_gtk.h"

#define g_signal_connect_data dyn_g_signal_connect_data

static void button_callback(GtkButton *button, void *nothing)
{
	void (*dyn_gtk_main_quit)(void);

	dyn_gtk_main_quit = nothing;

	dyn_gtk_main_quit();
}

static int window_delete_callback(GtkWidget *window, GdkEvent *event, void *nothing)
{
	void (*dyn_gtk_main_quit)(void);

	dyn_gtk_main_quit = nothing;

	dyn_gtk_main_quit();

	return TRUE;
}

static void Sys_Error_GTK_DisplayError_Real(const char *error)
{
	void *gtk;
	void *gobject;
	GtkWidget *window;
	GtkWidget *text;
	GtkWidget *button;
	GtkWidget *frame;
	GtkWidget *horizontal_box;
	GtkTextBuffer *text_buffer;
	int argc = 1;
	char *fooargv[] = { "fodquake", 0 }; 
	char **argv = fooargv;

	void (*dyn_gtk_init)(int *argc, char ***argv);
	void (*dyn_gtk_main)(void);
	void (*dyn_gtk_main_quit)(void);

	GtkWidget *(*dyn_gtk_window_new)(GtkWindowType type);
	void (*dyn_gtk_window_set_title)(GtkWindow *window, const gchar *title);

	void (*dyn_gtk_widget_show)(GtkWidget *widget);
	void (*dyn_gtk_widget_hide)(GtkWidget *widget);
	void (*dyn_gtk_widget_destroy)(GtkWidget *widget);

	GtkWidget *(*dyn_gtk_button_new_with_label)(const gchar *label);

	GtkTextBuffer *(*dyn_gtk_text_buffer_new)(GtkTextTagTable *table);
	void (*dyn_gtk_text_buffer_insert_at_cursor)(GtkTextBuffer *buffer, const gchar *text, gint len);

	GtkWidget *(*dyn_gtk_text_view_new_with_buffer)(GtkTextBuffer *buffer);
	void (*dyn_gtk_text_view_set_editable)(GtkTextView *text_view, gboolean setting);

	void (*dyn_gtk_container_add)(GtkContainer *container, GtkWidget *widget);
	void (*dyn_gtk_container_set_border_width)(GtkContainer *container, guint border_width);

	GtkWidget *(*dyn_gtk_vbox_new)(gboolean homogeneous, gint spacing);

	void (*dyn_gtk_box_pack_start)(GtkBox *box, GtkWidget *child, gboolean expand, gboolean fill, guint padding);
	void (*dyn_gtk_box_pack_end)(GtkBox *box, GtkWidget *child, gboolean expand, gboolean fill, guint padding);
	void (*dyn_gtk_box_set_spacing)(GtkBox *box, gint spacing);

	GtkWidget *(*dyn_gtk_frame_new)(const gchar *label);

	gulong (*dyn_g_signal_connect_data)(gpointer instance, const gchar *detailed_signal, GCallback c_handler, gpointer data, GClosureNotify destroy_data, GConnectFlags connect_flags);
	void (*dyn_g_object_unref)(gpointer object);

	gtk = dlopen("libgtk-x11-2.0.so.0", RTLD_NOW);
	if (gtk)
	{
		gobject = dlopen("libgobject-2.0.so.0", RTLD_NOW);
		if (gobject)
		{
			dyn_gtk_init = dlsym(gtk, "gtk_init");
			dyn_gtk_main = dlsym(gtk, "gtk_main");
			dyn_gtk_main_quit = dlsym(gtk, "gtk_main_quit");
			dyn_gtk_window_new = dlsym(gtk, "gtk_window_new");
			dyn_gtk_window_set_title = dlsym(gtk, "gtk_window_set_title");
			dyn_gtk_widget_show = dlsym(gtk, "gtk_widget_show");
			dyn_gtk_widget_hide = dlsym(gtk, "gtk_widget_hide");
			dyn_gtk_widget_destroy = dlsym(gtk, "gtk_widget_destroy");
			dyn_gtk_button_new_with_label = dlsym(gtk, "gtk_button_new_with_label");
			dyn_gtk_text_buffer_new = dlsym(gtk, "gtk_text_buffer_new");
			dyn_gtk_text_buffer_insert_at_cursor = dlsym(gtk, "gtk_text_buffer_insert_at_cursor");
			dyn_gtk_text_view_new_with_buffer = dlsym(gtk, "gtk_text_view_new_with_buffer");
			dyn_gtk_text_view_set_editable = dlsym(gtk, "gtk_text_view_set_editable");
			dyn_gtk_container_add = dlsym(gtk, "gtk_container_add");
			dyn_gtk_container_set_border_width = dlsym(gtk, "gtk_container_set_border_width");
			dyn_gtk_vbox_new = dlsym(gtk, "gtk_vbox_new");
			dyn_gtk_box_pack_start = dlsym(gtk, "gtk_box_pack_start");
			dyn_gtk_box_pack_end = dlsym(gtk, "gtk_box_pack_end");
			dyn_gtk_box_set_spacing = dlsym(gtk, "gtk_box_set_spacing");
			dyn_gtk_frame_new = dlsym(gtk, "gtk_frame_new");

			dyn_g_signal_connect_data = dlsym(gobject, "g_signal_connect_data");
			dyn_g_object_unref = dlsym(gobject, "g_object_unref");

			if (dyn_gtk_init
			 && dyn_gtk_main
			 && dyn_gtk_main_quit
			 && dyn_gtk_window_new
			 && dyn_gtk_window_set_title
			 && dyn_gtk_widget_show
			 && dyn_gtk_widget_hide
			 && dyn_gtk_widget_destroy
			 && dyn_gtk_button_new_with_label
			 && dyn_gtk_text_buffer_new
			 && dyn_gtk_text_buffer_insert_at_cursor
			 && dyn_gtk_text_view_new_with_buffer
			 && dyn_gtk_container_add
			 && dyn_gtk_container_set_border_width
			 && dyn_gtk_vbox_new
			 && dyn_gtk_box_pack_start
			 && dyn_gtk_box_pack_end
			 && dyn_gtk_box_set_spacing
			 && dyn_gtk_frame_new
			 && dyn_g_signal_connect_data
			 && dyn_g_object_unref)
			{
				dyn_gtk_init(&argc, &argv);

				window = dyn_gtk_window_new(GTK_WINDOW_TOPLEVEL);
				if (window)
				{
					dyn_gtk_window_set_title(GTK_WINDOW(window), "Fodquake error");
					dyn_gtk_container_set_border_width(GTK_CONTAINER(window), 5);
					g_signal_connect(GTK_OBJECT(window), "delete-event", GTK_SIGNAL_FUNC(window_delete_callback), dyn_gtk_main_quit);

					frame = dyn_gtk_frame_new(NULL);
					if (frame)
					{
						dyn_gtk_container_add(GTK_CONTAINER(window), frame);

						horizontal_box = dyn_gtk_vbox_new(FALSE, 0);
						if (horizontal_box)
						{
							dyn_gtk_container_add(GTK_CONTAINER(frame), horizontal_box);

							text_buffer = dyn_gtk_text_buffer_new(NULL);
							if (text_buffer)
							{
								dyn_gtk_text_buffer_insert_at_cursor(text_buffer, error, -1);
	
								text = dyn_gtk_text_view_new_with_buffer(text_buffer);
								if (text)
								{
									dyn_gtk_text_view_set_editable(GTK_TEXT_VIEW(text), 0);

									button = dyn_gtk_button_new_with_label("OK");
									if (button)
									{
										g_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(button_callback), dyn_gtk_main_quit);

										dyn_gtk_box_set_spacing(GTK_BOX(horizontal_box), 12);
									        dyn_gtk_box_pack_start(GTK_BOX(horizontal_box), text, FALSE, FALSE, 5);
									        dyn_gtk_box_pack_end(GTK_BOX(horizontal_box), button, FALSE, FALSE, 5);

										dyn_gtk_widget_show(window);
										dyn_gtk_widget_show(frame);
										dyn_gtk_widget_show(horizontal_box);
										dyn_gtk_widget_show(button);
										dyn_gtk_widget_show(text);

										dyn_gtk_main();

										dyn_gtk_widget_hide(text);
										dyn_gtk_widget_hide(button);
										dyn_gtk_widget_hide(horizontal_box);
										dyn_gtk_widget_hide(frame);
										dyn_gtk_widget_hide(window);

										dyn_gtk_widget_destroy(button);
									}

									dyn_gtk_widget_destroy(text);
								}

								dyn_g_object_unref(text_buffer);
							}

							dyn_gtk_widget_destroy(horizontal_box);
						}

						dyn_gtk_widget_destroy(frame);
					}

					dyn_gtk_widget_destroy(window);
				}
			}

			dlclose(gobject);
		}

		dlclose(gtk);
	}

	/* Yes, GTK always leaks :( */
}

void Sys_Error_GTK_DisplayError(const char *error)
{
	pid_t pid;

	pid = fork();
	if (pid == 0)
		Sys_Error_GTK_DisplayError_Real(error);
	else if (pid > 0)
	{
		waitpid(pid, 0, 0);
	}
}

#ifdef TEST
int main()
{
	Sys_GTK_DisplayError("Random error!");

	return 0;
}
#endif

