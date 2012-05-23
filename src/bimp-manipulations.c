/*
 * Functions to initialize and manage manipulations
 */

#include <string.h>
#include <gtk/gtk.h>
#include "bimp-manipulations.h"
#include "bimp-manipulations-gui.h"
#include "bimp-icons.h"

static manipulation manipulation_resize_new(void);
static manipulation manipulation_crop_new(void);
static manipulation manipulation_fliprotate_new(void);
static manipulation manipulation_color_new(void);
static manipulation manipulation_sharpblur_new(void);
static manipulation manipulation_watermark_new(void); 
static manipulation manipulation_changeformat_new(void); 
static manipulation manipulation_rename_new(void); 
static manipulation manipulation_userdef_new(void); 


/* Appends a default manipulation to the step list */
manipulation bimp_append_manipulation(manipulation_type type)
{
	manipulation newman = NULL; /* newman, paul newman. */
	
	if (type != MANIP_USERDEF && bimp_list_contains_manip(type)) {
		return NULL;
	}
	else {
		if (type == MANIP_RESIZE) {
			newman = manipulation_resize_new();
		}
		else if (type == MANIP_CROP) {
			newman = manipulation_crop_new();
		}
		else if (type == MANIP_FLIPROTATE) {
			newman = manipulation_fliprotate_new();
		}
		else if (type == MANIP_COLOR) {
			newman = manipulation_color_new();
		}
		else if (type == MANIP_SHARPBLUR) {
			newman = manipulation_sharpblur_new();
		}
		else if (type == MANIP_WATERMARK) {
			newman = manipulation_watermark_new();
		}
		else if (type == MANIP_CHANGEFORMAT) {
			newman = manipulation_changeformat_new();
		}
		else if (type == MANIP_RENAME) {
			newman = manipulation_rename_new();
		}
		else if (type == MANIP_USERDEF) {
			newman = manipulation_userdef_new();
		}
				
		bimp_selected_manipulations = g_slist_append(bimp_selected_manipulations, newman);
		
		return newman;
	}
}

/* remove a manipulation from the list */
void bimp_remove_manipulation(manipulation man)
{
	bimp_selected_manipulations = g_slist_remove(bimp_selected_manipulations, man);
}

/* check if at least one manipulation of type 'search' has been added to the list */
gboolean bimp_list_contains_manip(manipulation_type search) 
{
	gboolean find = FALSE;
	GSList *iterator = NULL;
	manipulation_type elem_type;
	
    for (iterator = bimp_selected_manipulations; iterator && find == FALSE; iterator = iterator->next) {
        elem_type = ((manipulation)(iterator->data))->type;
        if (elem_type == search) find = TRUE;
    }
    
    return find;
}

/* get the first occurrence of manipulation of type 'search' from the selected ones */
manipulation bimp_list_get_manip(manipulation_type search) 
{
	gboolean find = FALSE;
	manipulation found_man = NULL;
	GSList *iterator = NULL;
	
    for (iterator = bimp_selected_manipulations; iterator && find == FALSE; iterator = iterator->next) {
        found_man = (manipulation)(iterator->data);
        if (found_man->type == search) find = TRUE;
    }
    
    return found_man;
}

/* set of constructors for each type of manipulation (with default values) */

static manipulation manipulation_resize_new() 
{
	manipulation resize;
	resize = (manipulation) g_malloc(sizeof(struct manip_str));
	resize->type = MANIP_RESIZE;
	resize->icon = &pixdata_resize;
	resize->settings = (resize_settings) g_malloc(sizeof(struct manip_resize_set));
	((resize_settings)resize->settings)->newWpc = 100.0;
	((resize_settings)resize->settings)->newHpc = 100.0;
	((resize_settings)resize->settings)->newWpx = 640;
	((resize_settings)resize->settings)->newHpx = 480;
	((resize_settings)resize->settings)->sizemode = RESIZE_PERCENT;
	((resize_settings)resize->settings)->aspect_ratio = TRUE;
	((resize_settings)resize->settings)->interpolation = GIMP_INTERPOLATION_CUBIC;
	((resize_settings)resize->settings)->change_res = FALSE;
	((resize_settings)resize->settings)->newResX = 72.000;
	((resize_settings)resize->settings)->newResY = 72.000;
	
	return resize;
}

static manipulation manipulation_crop_new() 
{
	manipulation crop;
	crop = (manipulation) g_malloc(sizeof(struct manip_str));
	crop->type = MANIP_CROP;
	crop->icon = &pixdata_crop;
	crop->settings = (crop_settings) g_malloc(sizeof(struct manip_crop_set));
	((crop_settings)crop->settings)->newW = 640;
	((crop_settings)crop->settings)->newH = 480;
	((crop_settings)crop->settings)->manual = FALSE;
	((crop_settings)crop->settings)->ratio = CROP_PRESET_11;
	
	return crop;
}

static manipulation manipulation_fliprotate_new() 
{
	manipulation fliprotate;
	fliprotate = (manipulation) g_malloc(sizeof(struct manip_str));
	fliprotate->type = MANIP_FLIPROTATE;
	fliprotate->icon = &pixdata_rotate;
	fliprotate->settings = (fliprotate_settings) g_malloc(sizeof(struct manip_fliprotate_set));
	((fliprotate_settings)fliprotate->settings)->flipH = FALSE;
	((fliprotate_settings)fliprotate->settings)->flipV = FALSE;
	((fliprotate_settings)fliprotate->settings)->rotate = FALSE;
	((fliprotate_settings)fliprotate->settings)->rotate_type = GIMP_ROTATE_90;
	
	return fliprotate;
}

static manipulation manipulation_color_new() 
{
	manipulation color;
	color = (manipulation) g_malloc(sizeof(struct manip_str));
	color->type = MANIP_COLOR;
	color->icon = &pixdata_color;
	color->settings = (color_settings) g_malloc(sizeof(struct manip_color_set));
	((color_settings)color->settings)->brightness = 0;
	((color_settings)color->settings)->contrast = 0;
	((color_settings)color->settings)->grayscale = FALSE;
	((color_settings)color->settings)->levels_auto = FALSE;
	
	return color;
}

static manipulation manipulation_sharpblur_new() 
{
	manipulation sharpblur;
	sharpblur = (manipulation) g_malloc(sizeof(struct manip_str));
	sharpblur->type = MANIP_SHARPBLUR;
	sharpblur->icon = &pixdata_sharp;
	sharpblur->settings = (sharpblur_settings) g_malloc(sizeof(struct manip_sharpblur_set));
	((sharpblur_settings)sharpblur->settings)->amount = 0;
	
	return sharpblur;
}

static manipulation manipulation_watermark_new() 
{
	manipulation watermark;
	watermark = (manipulation) g_malloc(sizeof(struct manip_str));
	watermark->type = MANIP_WATERMARK;
	watermark->icon = &pixdata_watermark;
	watermark->settings = (watermark_settings) g_malloc(sizeof(struct manip_watermark_set));
	((watermark_settings)watermark->settings)->textmode = TRUE;
	((watermark_settings)watermark->settings)->text = "";
	((watermark_settings)watermark->settings)->font = pango_font_description_copy(pango_font_description_from_string("Sans 16px"));
	gdk_color_parse("black", &(((watermark_settings)watermark->settings)->color));
	gdk_colormap_alloc_color(gdk_colormap_get_system(), &(((watermark_settings)watermark->settings)->color), TRUE, TRUE);
	((watermark_settings)watermark->settings)->imagefile = NULL;
	((watermark_settings)watermark->settings)->opacity = 100.0;
	((watermark_settings)watermark->settings)->position = WM_POS_BR;
	
	return watermark;
}

static manipulation manipulation_changeformat_new() 
{
	manipulation changeformat;
	changeformat = (manipulation) g_malloc(sizeof(struct manip_str));
	changeformat->type = MANIP_CHANGEFORMAT;
	changeformat->icon = &pixdata_changeformat;
	changeformat->settings = (changeformat_settings) g_malloc(sizeof(struct manip_changeformat_set));
	((changeformat_settings)changeformat->settings)->format = FORMAT_JPEG;
	((changeformat_settings)changeformat->settings)->params = NULL;
	((changeformat_settings)changeformat->settings)->params = (format_params_jpeg) g_malloc(sizeof(struct changeformat_params_jpeg));
	((format_params_jpeg)((changeformat_settings)changeformat->settings)->params)->quality = 85.0;
	((format_params_jpeg)((changeformat_settings)changeformat->settings)->params)->smoothing = 0.0;
	((format_params_jpeg)((changeformat_settings)changeformat->settings)->params)->entropy = TRUE;
	((format_params_jpeg)((changeformat_settings)changeformat->settings)->params)->progressive = FALSE;
	((format_params_jpeg)((changeformat_settings)changeformat->settings)->params)->comment = "";
	((format_params_jpeg)((changeformat_settings)changeformat->settings)->params)->subsampling = 3;
	((format_params_jpeg)((changeformat_settings)changeformat->settings)->params)->baseline = TRUE;
	((format_params_jpeg)((changeformat_settings)changeformat->settings)->params)->markers = 0;
	((format_params_jpeg)((changeformat_settings)changeformat->settings)->params)->dct = 1;
	
	return changeformat;
}

static manipulation manipulation_rename_new() 
{
	manipulation rename;
	rename = (manipulation) g_malloc(sizeof(struct manip_str));
	rename->type = MANIP_RENAME;
	rename->icon = &pixdata_rename;
	rename->settings = (rename_settings) g_malloc(sizeof(struct manip_rename_set));
	((rename_settings)rename->settings)->pattern = RENAME_KEY_ORIG;
	
	return rename;
}

static manipulation manipulation_userdef_new() 
{
	manipulation userdef;
	userdef = (manipulation) g_malloc(sizeof(struct manip_str));
	userdef->type = MANIP_USERDEF;
	userdef->icon = &pixdata_userdef;
	userdef->settings = (userdef_settings) g_malloc(sizeof(struct manip_userdef_set));
	((userdef_settings)userdef->settings)->procedure = NULL;
	((userdef_settings)userdef->settings)->num_params = 0;
	((userdef_settings)userdef->settings)->params = NULL;
	
	return userdef;
}

