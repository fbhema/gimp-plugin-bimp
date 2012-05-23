#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include "gui-userdef.h"
#include "../bimp.h"
#include "../bimp-gui.h"
#include "../bimp-manipulations.h"
#include "../bimp-utils.h"

static void init_procedure_list(void);
static int fill_procedure_list(char*, char*);
static gboolean proc_has_compatible_params(gchar*); 
static void search_procedure (GtkEditable*, gpointer);
static GimpParamDef get_param_info(gchar*, gint);
static gboolean select_procedure (GtkTreeSelection*, GtkTreeModel*, GtkTreePath*, gboolean, gpointer);
static void update_selected_procedure(gchar*);
static void update_procedure_box(userdef_settings);

GtkWidget *treeview_procedures;
GtkWidget *scroll_procparam, *vbox_procparam;
GtkWidget *parent_dialog;

userdef_settings temp_settings;
GtkWidget **param_widget; /* array of showable widgets for customizing the selected procedure */

GtkTreeSelection *treesel_proc;

enum
{
  LIST_ITEM = 0,
  N_COLUMNS
};

GtkWidget* bimp_userdef_gui_new(userdef_settings settings, GtkWidget *parent)
{
	GtkWidget *gui, *hbox_chooser, *vbox_list, *hbox_search;
	GtkWidget *scroll_procedures;
	GtkWidget *label_help, *label_search;
	GtkWidget *entry_search;
	
	parent_dialog = parent;
	
	gui = gtk_vbox_new(FALSE, 5);
		
	label_help = gtk_label_new(NULL);
	gtk_widget_set_size_request (label_help, LABEL_HELP_W, LABEL_HELP_H);
	gtk_label_set_markup (GTK_LABEL(label_help), 
		"Choose a supported GIMP procedure from the list on the left\n"
		"and define its parameters on the right."
	);
	gtk_label_set_justify(GTK_LABEL(label_help), GTK_JUSTIFY_CENTER);
	
	hbox_chooser = gtk_hbox_new(FALSE, 5);
	vbox_list = gtk_vbox_new(FALSE, 5);
	
	scroll_procedures = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_procedures), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_size_request (scroll_procedures, PROCLIST_W, PROCLIST_H);
	
	hbox_search = gtk_hbox_new(FALSE, 5);
	label_search = gtk_label_new("Search: ");
	entry_search = gtk_entry_new();
	gtk_widget_set_size_request (entry_search, SEARCH_W, SEARCH_H);
	
	treeview_procedures = gtk_tree_view_new();
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview_procedures), FALSE);
	init_procedure_list();
	int sel_index = fill_procedure_list(NULL, settings->procedure);
	treesel_proc = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview_procedures));
	gtk_tree_selection_set_select_function (treesel_proc, select_procedure, NULL, NULL);
	
	scroll_procparam = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_procparam), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_size_request (scroll_procparam, PROCPARAM_W, PROCPARAM_H);
	
	gtk_box_pack_start(GTK_BOX(hbox_search), label_search, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox_search), entry_search, FALSE, FALSE, 0);
	
	gtk_box_pack_start(GTK_BOX(vbox_list), hbox_search, FALSE, FALSE, 0);
	gtk_container_add (GTK_CONTAINER(scroll_procedures), treeview_procedures);
	gtk_box_pack_start(GTK_BOX(vbox_list), scroll_procedures, FALSE, FALSE, 0);
	
	gtk_box_pack_start(GTK_BOX(hbox_chooser), vbox_list, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox_chooser), scroll_procparam, FALSE, FALSE, 0);
	
	gtk_box_pack_start(GTK_BOX(gui), label_help, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(gui), hbox_chooser, FALSE, FALSE, 0);
	
	if (settings->procedure != NULL) { /* a procedure has been previously selected: copy user settings to temp_settings */
		temp_settings = (userdef_settings)g_malloc(sizeof(struct manip_userdef_set));

		temp_settings->procedure = g_strdup(settings->procedure);
		temp_settings->num_params = settings->num_params;

		temp_settings->params = g_new(GimpParam, settings->num_params);
		
		int param_i;
		for (param_i = 0; param_i < temp_settings->num_params; param_i++) {
			temp_settings->params[param_i].type = settings->params[param_i].type;
			temp_settings->params[param_i].data = settings->params[param_i].data;
		}
		
		/* and select the relative TreeView entry */
		GtkTreePath *path;
		path = gtk_tree_path_new_from_indices(sel_index, -1);
		gtk_tree_selection_select_path(treesel_proc, path);      
		gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(treeview_procedures), path, NULL, TRUE, 0.5, 0.0);
	}
	
	update_procedure_box(settings); 
	
	g_signal_connect(G_OBJECT(entry_search), "changed", G_CALLBACK(search_procedure), NULL);
	
	return gui;
}

/* initializes the GtkTreeView */
static void init_procedure_list() 
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkListStore *store;

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
		"List Items",
		renderer, 
		"text", 
		LIST_ITEM, 
		NULL
	);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview_procedures), column);

	store = gtk_list_store_new(N_COLUMNS, G_TYPE_STRING);
	gtk_tree_view_set_model(GTK_TREE_VIEW(treeview_procedures), GTK_TREE_MODEL(store));
	g_object_unref(store);
}

/* fills the treeview with only those procedures that can be used for a batch operation.
 * the filtering is made in two steps: 
 *  - by ignoring system's procedures 
 *  - by ignoring procedures that contains non-compatible parameters datatypes (like FLOATARRAY, PATH, ...).
 * the functions supports also a secondary filter for user searches */
static int fill_procedure_list(char* search, char* selection) 
{
	gint proc_count;
	gchar** results;
	
	GtkListStore *store;
	GtkTreeModel *model;
	GtkTreeIter treeiter;
	
	if (search != NULL && strlen(search) > 0) {
		search = g_strconcat(".*", search, NULL);
	} else {
		search = ".*";
	}
	
	gimp_procedural_db_query (
		g_strconcat(
		"^(?!.*(?:"
			"plug-in-bimp|"
			"extension-|"
			"-get-|"
			"-set-|"
			"-is-|"
			"-has-|"
			"-get-|"
			"-load|"
			"-save|"
			"-free|"
			"-help|"
			"-temp|"
			"-undo|"
			"-channel|"
			"-buffer|"
			"-register|"
			"-metadata|"
			"-layer|"
			"-selection|"
			"-brush|"
			"-gradient|"
			"-guide|"
			"-parasite|"
			"gimp-online|"
			"gimp-progress|"
			"gimp-procedural|"
			"gimp-display|"
			"gimp-context|"
			"gimp-edit|"
			"gimp-fonts|"
			"gimp-palette|"
			"gimp-path|"
			"gimp-pattern|"
			"gimp-vectors|"
			"gimp-quit|"
			"gimp-plugins|"
			"gimp-gimprc|"
			"temp-procedure"
		"))", search, NULL), // adds search filter if any
		".*",
		".*",
		".*",
		".*",
		".*",
		".*",
		&proc_count,
		&results
	);
	
	qsort(results, proc_count, sizeof(char*), cstring_cmp); /* sort alphabetically */
	
	store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW (treeview_procedures)));
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview_procedures));
	
	/* clear all rows in list */
	if (gtk_tree_model_get_iter_first(model, &treeiter) == TRUE) {
		gtk_list_store_clear(store);
	}
	
	int i, finalcount = 0, selected_i = -1;
	for (i = 0; i < proc_count; i++) {
		/* check each parameter for compatibility */
		if (proc_has_compatible_params(results[i])) {
			store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(treeview_procedures)));
			gtk_list_store_append(store, &treeiter);
			gtk_list_store_set(store, &treeiter, LIST_ITEM, results[i], -1);
			
			if (selected_i != finalcount && selection != NULL && strcmp(results[i], selection) == 0) {
				selected_i = finalcount;
			}
			
			finalcount++;
		}
	}
	
	return selected_i;
}

static gboolean proc_has_compatible_params(gchar* proc_name) 
{
	gboolean compatible = TRUE;
	
	gchar* proc_blurb;
	gchar* proc_help;
	gchar* proc_author;
	gchar* proc_copyright;
	gchar* proc_date;
	GimpPDBProcType proc_type;
	gint num_params;
    gint num_values;
    GimpParamDef *params;
    GimpParamDef *return_vals;
	
	gimp_procedural_db_proc_info (
		proc_name,
		&proc_blurb,
		&proc_help,
		&proc_author,
		&proc_copyright,
		&proc_date,
		&proc_type,
		&num_params,
		&num_values,
		&params,
		&return_vals
	);
	
	int i;
	GimpParamDef param;
	for (i = 0; i < num_params && compatible; i++) {
		param = get_param_info(proc_name, i);
		
		if (
			param.type == GIMP_PDB_INT32 ||
			param.type == GIMP_PDB_INT16 ||
			param.type == GIMP_PDB_INT8 ||
			param.type == GIMP_PDB_FLOAT ||
			(param.type == GIMP_PDB_STRING && strstr(param.name, "filename") == NULL) ||
			param.type == GIMP_PDB_COLOR ||
			param.type == GIMP_PDB_DRAWABLE ||
			param.type == GIMP_PDB_IMAGE
			) {
			compatible = TRUE;
		} else {
			compatible = FALSE;
		}
	}
	
	return (compatible && num_params > 0);
}

static void search_procedure (GtkEditable *editable, gpointer data)  {
	fill_procedure_list((char*)g_strdup(gtk_entry_get_text(GTK_ENTRY(editable))), NULL);
}

static GimpParamDef get_param_info(gchar* proc_name, gint arg_num) 
{
	GimpParamDef param_info;
	GimpPDBArgType type;
	gchar *name;
	gchar *desc;
		
	gimp_procedural_db_proc_arg (
		proc_name,
		arg_num,
		&type,
		&name,
		&desc
	);
	
	param_info.type = type;
	param_info.name = g_strdup(name);
	param_info.description = g_strdup(desc);
	
	return param_info;
}

/* called when a row is going to be selected or unselected 
 * if it's going to be selected, it updates the widget panel */
static gboolean select_procedure (GtkTreeSelection *selection, GtkTreeModel *model, GtkTreePath *path, gboolean path_currently_selected, gpointer data) 
{
	GtkTreeIter iter;
	gchar *selected;   
	
	if (gtk_tree_model_get_iter(model, &iter, path)) {
		gtk_tree_model_get(model, &iter, LIST_ITEM, &selected,  -1);
		
		if (!path_currently_selected) { /* going to be selected... */
			update_selected_procedure(selected);
		}
		
		return TRUE;
	} else {
		return FALSE;
	}
}

/* called when the users chooses a new procedure from the list. 
 * it fills the temp settings with default values and calls the panel update function */
static void update_selected_procedure(gchar* procedure) 
{
	if (temp_settings != NULL && strcmp(temp_settings->procedure, procedure) == 0) {
		return;
	}
	
	gchar* proc_blurb;
	gchar* proc_help;
	gchar* proc_author;
	gchar* proc_copyright;
	gchar* proc_date;
	GimpPDBProcType proc_type;
	gint num_params;
    gint num_values;
    GimpParamDef *params;
    GimpParamDef *return_vals;
	
	gimp_procedural_db_proc_info (
		procedure,
		&proc_blurb,
		&proc_help,
		&proc_author,
		&proc_copyright,
		&proc_date,
		&proc_type,
		&num_params,
		&num_values,
		&params,
		&return_vals
	);
	
	g_free(temp_settings);
	temp_settings = (userdef_settings)g_malloc(sizeof(struct manip_userdef_set));
	
	temp_settings->procedure = g_strdup(procedure);
	temp_settings->num_params = num_params;
	
	temp_settings->params = g_new(GimpParam, num_params);
	
	GimpRGB tempcolor;
	gimp_rgb_set(&tempcolor, 0.0, 0.0, 0.0);
	
	int param_i;
	for(param_i = 0; param_i < temp_settings->num_params; param_i++) {
		temp_settings->params[param_i].type = params[param_i].type;
		
		/* set default values */
		switch(temp_settings->params[param_i].type) {
			case GIMP_PDB_INT32:
				temp_settings->params[param_i].data.d_int32 = 0;
			case GIMP_PDB_INT16:
				temp_settings->params[param_i].data.d_int16 = 0;
			case GIMP_PDB_INT8:
				temp_settings->params[param_i].data.d_int8 = 0;
			case GIMP_PDB_FLOAT: 
				if (strcmp(params[param_i].name, "opacity") == 0) {
					temp_settings->params[param_i].data.d_float = 100.0;
				} else {
					temp_settings->params[param_i].data.d_float = 0.0;
				}
				break;
			case GIMP_PDB_STRING: 
				if (strcmp(params[param_i].name, "font") == 0) {
					temp_settings->params[param_i].data.d_string = "Sans 16px";
				} else {
					temp_settings->params[param_i].data.d_string = "";
				}
				break;
			case GIMP_PDB_COLOR:
				temp_settings->params[param_i].data.d_color = tempcolor;
				break;
				
			default: 
				break;
		}
	}
	
	update_procedure_box(temp_settings);
}

/* fills the right box with the given procedure's information and settings */
static void update_procedure_box(userdef_settings settings) 
{
	GtkWidget *vbox_procparam;
	GtkWidget *scroll_contents;
	
	scroll_contents = gtk_bin_get_child(GTK_BIN(scroll_procparam));
	if (scroll_contents != NULL) {
		gtk_widget_destroy(scroll_contents);
	}
	
	vbox_procparam = gtk_vbox_new(FALSE, 5);
		
	if (settings->procedure == NULL) {
		GtkWidget *label_noproc;
		
		label_noproc = gtk_label_new("Can't save because\nno procedure has been selected");
		gtk_widget_set_size_request (label_noproc, PROCLIST_W, PROCLIST_H);
		gtk_label_set_justify(GTK_LABEL(label_noproc), GTK_JUSTIFY_CENTER);
		
		gtk_box_pack_start(GTK_BOX(vbox_procparam), label_noproc, FALSE, FALSE, 0);
		
		gtk_dialog_set_response_sensitive (GTK_DIALOG(parent_dialog), GTK_RESPONSE_ACCEPT, FALSE);
	}
	else {
		gchar *proc_desc, *proc_help, *proc_author, *proc_copyright, *proc_date;
		GimpPDBProcType proc_type;
		gint num_params, num_values;
		GimpParamDef *params;
		GimpParamDef *return_vals;
    
		GtkWidget *label_procname, *label_procdescr, *label_procauthor;
		
		/* get procedure's general info*/
		gimp_procedural_db_proc_info (
			settings->procedure,
			&proc_desc,
			&proc_help,
			&proc_author,
			&proc_copyright,
			&proc_date,
			&proc_type,
			&num_params,
			&num_values,
			&params,
			&return_vals
		);
		
		/* box header with name, description and author */
		
		label_procname = gtk_label_new(NULL);
		gtk_label_set_markup (GTK_LABEL(label_procname), g_strconcat("<b>", settings->procedure, "</b>", NULL));
		gtk_misc_set_alignment(GTK_MISC(label_procname), 0, 0);
		
		label_procdescr = gtk_label_new(NULL);
		if (proc_desc != NULL) {
			gtk_label_set_markup (GTK_LABEL(label_procdescr), g_strconcat("<i>", g_markup_escape_text(proc_desc, -1), "</i>", NULL));
			gtk_misc_set_alignment(GTK_MISC(label_procdescr), 0, 0);
		}
		
		label_procauthor = gtk_label_new(NULL);
		if (proc_author != NULL) {
			gtk_label_set_markup (GTK_LABEL(label_procauthor), g_strconcat("Author: ", g_markup_escape_text(proc_author, -1), NULL));
			gtk_misc_set_alignment(GTK_MISC(label_procauthor), 0, 0);
		}
		
		gtk_box_pack_start(GTK_BOX(vbox_procparam), label_procname, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(vbox_procparam), label_procdescr, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(vbox_procparam), label_procauthor, FALSE, FALSE, 5);
		
		/* widgets for the params */
		
		g_free(param_widget);
		param_widget = g_new(GtkWidget*, num_params);
		
		int param_i, show_count = 0;
		GimpParamDef param_info;
		GdkColor usercolor;
		gboolean editable;
		GtkWidget *label_widget_desc, *hbox_paramrow;
		
		for (param_i = 0; param_i < num_params; param_i++) {
			param_info = get_param_info(settings->procedure, param_i);
			editable = TRUE;
			
			switch(param_info.type) {
				case GIMP_PDB_INT32:
					if (strcmp(param_info.name, "run-mode") == 0) {
						editable = FALSE;
					}
					else {
						if (strcmp(param_info.name, "toggle") == 0 || (bimp_str_contains_cins(param_info.description, "true") && bimp_str_contains_cins(param_info.description, "false"))) {
							/* if it's named 'toggle' or contains TRUE/FALSE pretend to be a boolean value */
							param_widget[param_i] = gtk_check_button_new();
							gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(param_widget[param_i]), (settings->params[param_i]).data.d_int32 == 1 ? TRUE : FALSE);
						} else {
							param_widget[param_i] = gtk_spin_button_new (NULL, 1, 0);
							gtk_spin_button_configure (GTK_SPIN_BUTTON(param_widget[param_i]), GTK_ADJUSTMENT(gtk_adjustment_new ((settings->params[param_i]).data.d_int32, -10000, 10000, 1, 1, 0)), 0, 0);
						}
					}
					break;

				case GIMP_PDB_INT16:
					param_widget[param_i] = gtk_spin_button_new (NULL, 1, 0);
					gtk_spin_button_configure (GTK_SPIN_BUTTON(param_widget[param_i]), GTK_ADJUSTMENT(gtk_adjustment_new ((settings->params[param_i]).data.d_int16, -10000, 10000, 1, 1, 0)), 0, 0);
					break;
					
				case GIMP_PDB_INT8:
					param_widget[param_i] = gtk_spin_button_new (NULL, 1, 0);
					gtk_spin_button_configure (GTK_SPIN_BUTTON(param_widget[param_i]), GTK_ADJUSTMENT(gtk_adjustment_new ((settings->params[param_i]).data.d_int8, 0, 255, 1, 1, 0)), 0, 0);
					break;
					
				case GIMP_PDB_FLOAT: 
					param_widget[param_i] = gtk_spin_button_new (NULL, 1, 1);
					if (strcmp(param_info.name, "opacity") == 0) {
						/* if the param is an opacity, its range goes from 0.0 to 100.0 */
						gtk_spin_button_configure (GTK_SPIN_BUTTON(param_widget[param_i]), GTK_ADJUSTMENT(gtk_adjustment_new ((float)((settings->params[param_i]).data.d_float), 0.0, 100.0, 0.1, 1, 0)), 0, 1);
					} else {
						gtk_spin_button_configure (GTK_SPIN_BUTTON(param_widget[param_i]), GTK_ADJUSTMENT(gtk_adjustment_new ((float)((settings->params[param_i]).data.d_float), -10000.0, 10000.0, 0.1, 1, 0)), 0, 1);
					}
					break;
					
				/* case: this param is a string, prepare a gtk_entry */
				case GIMP_PDB_STRING: 
					if (strcmp(param_info.name, "font") == 0){
						param_widget[param_i] = gtk_font_button_new_with_font((settings->params[param_i]).data.d_string);
					}
					else {
						param_widget[param_i] = gtk_entry_new();
						gtk_entry_set_text (GTK_ENTRY(param_widget[param_i]), (settings->params[param_i]).data.d_string);
					}
					break;
				
				/* case: this param is a color, prepare a color chooser */
				case GIMP_PDB_COLOR: 
					usercolor.red = (guint16)(((settings->params[param_i]).data.d_color.r)*65535);
					usercolor.green = (guint16)(((settings->params[param_i]).data.d_color.g)*65535);
					usercolor.blue = (guint16)(((settings->params[param_i]).data.d_color.b)*65535);
					param_widget[param_i] = gtk_color_button_new_with_color(&usercolor);
					break;
					
				case GIMP_PDB_DRAWABLE:
				case GIMP_PDB_IMAGE:
					editable = FALSE; 
					break;
				
				default: 
					editable = FALSE; 
					break;
			}
			
			if (editable) {
				gtk_widget_set_size_request (param_widget[param_i], PARAM_WIDGET_W, PARAM_WIDGET_H);
				label_widget_desc = gtk_label_new(param_info.description);
				gtk_widget_set_tooltip_text (label_widget_desc, param_info.name);
				gtk_misc_set_alignment(GTK_MISC(label_widget_desc), 0, 0.5);
				
				hbox_paramrow = gtk_hbox_new(FALSE, 5);
				gtk_box_pack_start(GTK_BOX(hbox_paramrow), param_widget[param_i], FALSE, FALSE, 0);
				gtk_box_pack_start(GTK_BOX(hbox_paramrow), label_widget_desc, FALSE, FALSE, 0);
				gtk_box_pack_start(GTK_BOX(vbox_procparam), hbox_paramrow, FALSE, FALSE, 0);
				
				show_count++;
			} else {
				param_widget[param_i] = NULL; /* this param is not editable, so there is no visualizable widget */
			}
		}
		
		if (show_count == 0) {
			GtkWidget* label_noparams;
			label_noparams = gtk_label_new("This procedure takes no editable params");
			gtk_widget_set_size_request (label_noparams, PROCPARAM_W, 50);
			
			gtk_box_pack_start(GTK_BOX(vbox_procparam), label_noparams, FALSE, FALSE, 0);
		}
		
		gtk_dialog_set_response_sensitive (GTK_DIALOG(parent_dialog), GTK_RESPONSE_ACCEPT, TRUE);
	}
	
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scroll_procparam), vbox_procparam);
	gtk_viewport_set_shadow_type(GTK_VIEWPORT(gtk_bin_get_child(GTK_BIN(scroll_procparam))), GTK_SHADOW_NONE);
	gtk_widget_show_all(scroll_procparam);
}

/* store all the data from temp settings into the user settings */
void bimp_userdef_save(userdef_settings orig_settings) 
{
	g_free(orig_settings->procedure);
	orig_settings->procedure = g_strdup(temp_settings->procedure);
	
	orig_settings->num_params = temp_settings->num_params;
	
	g_free(orig_settings->params);
	orig_settings->params = g_new(GimpParam, orig_settings->num_params);
	
	int param_i;
	GimpParamDef param_info;
	GdkColor usercolor;
	GimpRGB rgbdata;
	for (param_i = 0; param_i < orig_settings->num_params; param_i++) {
		param_info = get_param_info(orig_settings->procedure, param_i);
		
		orig_settings->params[param_i].type = temp_settings->params[param_i].type;
		
		switch(orig_settings->params[param_i].type) {
			case GIMP_PDB_INT32:
				if (strcmp(param_info.name, "run-mode") == 0) {
					(orig_settings->params[param_i]).data.d_int32 = (gint32)GIMP_RUN_NONINTERACTIVE;
				} else if (strcmp(param_info.name, "toggle") == 0 || (bimp_str_contains_cins(param_info.description, "true") && bimp_str_contains_cins(param_info.description, "false"))) {
					(orig_settings->params[param_i]).data.d_int32 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(param_widget[param_i])) ? 1 : 0;
				}
				else {
					(orig_settings->params[param_i]).data.d_int32 = (gint32)gtk_spin_button_get_value(GTK_SPIN_BUTTON(param_widget[param_i]));
				}
				break;
				
			case GIMP_PDB_INT16:
				(orig_settings->params[param_i]).data.d_int16 = (gint16)gtk_spin_button_get_value(GTK_SPIN_BUTTON(param_widget[param_i]));
				break;
				
			case GIMP_PDB_INT8:
				(orig_settings->params[param_i]).data.d_int8 = (gint8)gtk_spin_button_get_value(GTK_SPIN_BUTTON(param_widget[param_i]));
				break;
				
			case GIMP_PDB_FLOAT: 
				(orig_settings->params[param_i]).data.d_float = (gdouble)gtk_spin_button_get_value(GTK_SPIN_BUTTON(param_widget[param_i]));
				break;
				
			case GIMP_PDB_STRING: 
				if (strcmp(param_info.name, "font") == 0) {
					(orig_settings->params[param_i]).data.d_string = g_strdup(gtk_font_button_get_font_name(GTK_FONT_BUTTON(param_widget[param_i])));
				} else {
					(orig_settings->params[param_i]).data.d_string = g_strdup(gtk_entry_get_text(GTK_ENTRY(param_widget[param_i])));
				}
				break;
			
			case GIMP_PDB_COLOR: 
				gtk_color_button_get_color(GTK_COLOR_BUTTON(param_widget[param_i]), &usercolor);
				gimp_rgb_set(&rgbdata, (gdouble)usercolor.red/65535, (gdouble)usercolor.green/65535, (gdouble)usercolor.blue/65535);
				(orig_settings->params[param_i]).data.d_color = rgbdata;
				break;
				
			default: break;
		}
	}
}

