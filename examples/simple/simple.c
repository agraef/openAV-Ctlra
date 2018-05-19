#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#include "ctlra.h"

#ifdef CTLRA_HAVE_CAIRO
#include <cairo/cairo.h>
#include "ctlra_cairo.h"
#else
/* small static compiled in bitmap library */
#include "sera.h"
#endif

static volatile uint32_t done;
static uint32_t led;
static uint32_t led_set;
static int redraw_screens = 1;

static float xoff;

void simple_feedback_func(struct ctlra_dev_t *dev, void *d)
{
	/* feedback like LEDs and Screen drawing based on application
	 * state goes here. See the vegas_mode/ example for an example of
	 * how to turn on LEDs on the controllers.
	 *
	 * Alternatively re-run the simple example with an integer
	 * parameter and that light number will be lit: ./simple 5
	 */
	if(led_set) {
		ctlra_dev_light_set(dev, led, 0xffffffff);
		ctlra_dev_light_flush(dev, 1);
	}
}

void simple_event_func(struct ctlra_dev_t* dev, uint32_t num_events,
		       struct ctlra_event_t** events, void *userdata)
{
	/* Events from the Ctlra device are handled here. They should be
	 * decoded, and events sent to the application. Note that this
	 * function must *NOT* send feedback to the device. Instead, the
	 * feedback_func() above should be used to send feedback. For a
	 * proper demo on how to do feedback, see examples/vegas/
	 */

	static const char* grid_pressed[] = { "   ", " X " };

	/* Retrieve info, so we can look up names. Note this is not
	 * expected to be used in production code - the names should be
	 * cached in the UI, and not retrieved on every event */
	struct ctlra_dev_info_t info;
	ctlra_dev_get_info(dev, &info);

	for(uint32_t i = 0; i < num_events; i++) {
		struct ctlra_event_t *e = events[i];
		const char *pressed = 0;
		const char *name = 0;
		switch(e->type) {
		case CTLRA_EVENT_BUTTON: {
			name = ctlra_info_get_name(&info, CTLRA_EVENT_BUTTON,
						   e->button.id);
			char pressure[16] = {0};
			if(e->button.has_pressure) {
				snprintf(pressure, sizeof(pressure),
					 "%0.01f", e->button.pressure);
			}
			printf("[%s] button %s (%d)\n",
				e->button.has_pressure ?  pressure :
				(e->button.pressed ? " X " : "   "),
				name, e->button.id);
			}
			if(e->button.pressed) {
				/* any button event to repain screen */
				redraw_screens = 1;
			}
			break;


		case CTLRA_EVENT_ENCODER:
			name = ctlra_info_get_name(&info, CTLRA_EVENT_ENCODER,
						   e->encoder.id);
			printf("[%s] encoder %s (%d)\n",
			       e->encoder.delta > 0 ? " ->" : "<- ",
			       name, e->button.id);
			if(e->encoder.flags & CTLRA_EVENT_ENCODER_FLAG_FLOAT) {
				xoff += e->encoder.delta_float * 100;
				printf("xoff = %f\n", xoff);
			} break;

		case CTLRA_EVENT_SLIDER:
			name = ctlra_info_get_name(&info, CTLRA_EVENT_SLIDER,
						   e->slider.id);
			printf("[%03d] slider %s (%d)\n",
			       (int)(e->slider.value * 100.f),
			       name, e->slider.id);
			break;

		case CTLRA_EVENT_GRID:
			name = ctlra_info_get_name(&info, CTLRA_EVENT_GRID,
			                                  e->grid.id);
			if(e->grid.flags & CTLRA_EVENT_GRID_FLAG_BUTTON) {
				pressed = grid_pressed[e->grid.pressed];
			} else {
				pressed = "---";
			}
			printf("[%s] grid %d", pressed, e->grid.pos);
			if(e->grid.flags & CTLRA_EVENT_GRID_FLAG_PRESSURE)
				printf(", pressure %1.3f", e->grid.pressure);
			printf("\n");
			break;
		default:
			break;
		};
	}
}

void sighndlr(int signal)
{
	done = 1;
	printf("\n");
}

void simple_remove_func(struct ctlra_dev_t *dev, int unexpected_removal,
			void *userdata)
{
	/* Notifies application of device removal, also allows cleanup
	 * of any device specific structures. See daemon/ example, where
	 * the MIDI I/O is cleaned up in the remove() function */
	struct ctlra_dev_info_t info;
	ctlra_dev_get_info(dev, &info);
	printf("simple: removing %s %s\n", info.vendor, info.device);
}

int32_t simple_screen_redraw_func(struct ctlra_dev_t *dev,
				  uint32_t screen_idx,
				  uint8_t *pixel_data,
				  uint32_t bytes,
				  struct ctlra_screen_zone_t *redraw_zone,
				  void *userdata)
{

#ifndef CTLRA_HAVE_CAIRO
	static sr_Buffer *buf;
	if(!buf) {
		buf = sr_newBuffer(480, 272);
	}
	//sr_loadPixels(buf, pixel_data, SR_FMT_ARGB);
	sr_Pixel p = {
		.word = 0,
	};

	sr_clear(buf, p);

	p.word = -1;
	int x = xoff + 20;
	sr_drawRect(buf, p, x, 20, 480 - 40, 272 - 40);

	uint16_t *scn = (uint16_t *)pixel_data;
	for(int j = 0; j < 272; j++) {
		for(int i = 0; i < 480; i++) {
			sr_Pixel px = sr_getPixel(buf, i, j);
			*scn++ = px.rgba.r;
		}
	}

	return (screen_idx == 0);
#else
	/* do drawing using your favorite toolkit here: Cairo / QT etc.
	 * Example: use OpenAV AVTKA toolkit for UI widgets, then pull out
	 * the cairo_surface_t from the AVTKA UI instance (it provides that
	 * function for you :) and call the Cairo->DeviceScreen helper
	 */

	/* TODO: how to get an appropriate FORMAT HEIGHT WIDTH? */
	static cairo_surface_t *img;
	static cairo_t *cr;
	if(!img) {
		/* TODO: cleanup img / cr */
		//img = cairo_image_surface_create(CAIRO_FORMAT_RGB24,
		img = cairo_image_surface_create(CAIRO_FORMAT_RGB16_565,
						 480, 272);
		cr = cairo_create(img);
	}

	/* blank out bg */
	cairo_set_source_rgb(cr, 0, 0, 0);
	cairo_rectangle(cr, 0, 0, 480, 272);
	cairo_fill(cr);

	int x = 32;
	int xoff = 62;

	cairo_set_source_rgb(cr, 1, 0, 0);
	cairo_rectangle(cr, x, 20, 40, 40);
	cairo_fill(cr);
	x += xoff;

	cairo_set_source_rgb(cr, 0, 1, 0);
	cairo_rectangle(cr, x, 20, 40, 40);
	cairo_fill(cr);
	x += xoff;

	cairo_set_source_rgb(cr, 0, 0, 1);
	cairo_rectangle(cr, x, 20, 40, 40);
	cairo_fill(cr);
	x += xoff;

	cairo_set_source_rgb(cr, 1, 1, 0);
	cairo_rectangle(cr, x, 20, 40, 40);
	cairo_fill(cr);
	x += xoff;

	cairo_set_source_rgb(cr, 1, 0, 1);
	cairo_rectangle(cr, x, 20, 40, 40);
	cairo_fill(cr);
	x += xoff;

	cairo_set_source_rgb(cr, 0, 1, 1);
	cairo_rectangle(cr, x, 20, 40, 40);
	cairo_fill(cr);
	x += xoff;

	cairo_set_source_rgb(cr, 1, 1, 1);
	cairo_rectangle(cr, x, 20, 40, 40);
	cairo_fill(cr);
	x += xoff;

	if(screen_idx == 0) {
		/* draw screen A */
	} else {
		/* draw screen B */
	}
	ctlra_screen_cairo_to_device(dev, screen_idx, pixel_data, bytes,
				     redraw_zone, img);

	/* return 1 to flush the screens - helper function above already
	 * handles this.
	 */
	return 1;
#endif
}

int accept_dev_func(struct ctlra_t *ctlra,
		    const struct ctlra_dev_info_t *info,
		    struct ctlra_dev_t *dev,
                    void *userdata)
{
	printf("simple: accepting %s %s\n", info->vendor, info->device);

	/* here we use the Ctlra APIs to set callback functions to get
	 * events and send feedback updates to/from the device */
	ctlra_dev_set_event_func(dev, simple_event_func);
	ctlra_dev_set_feedback_func(dev, simple_feedback_func);
	ctlra_dev_set_screen_feedback_func(dev, simple_screen_redraw_func);
	ctlra_dev_set_remove_func(dev, simple_remove_func);
	ctlra_dev_set_callback_userdata(dev, 0x0);

	return 1;
}

int main(int argc, char **argv)
{
	if(argc > 1) {
		led = atoi(argv[1]);
		led_set = 1;
	}

	signal(SIGINT, sighndlr);

	struct ctlra_t *ctlra = ctlra_create(NULL);
	int num_devs = ctlra_probe(ctlra, accept_dev_func, 0x0);
	printf("connected devices %d\n", num_devs);

	while(!done) {
		ctlra_idle_iter(ctlra);
		usleep(10 * 1000);
	}

	ctlra_exit(ctlra);

	return 0;
}
