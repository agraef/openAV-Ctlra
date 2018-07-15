#define _DEFAULT_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#include "ctlra.h"

static volatile uint32_t done;
static uint32_t led_count;

#define NUM_LEDS 256
static uint32_t leds[NUM_LEDS];
static uint32_t leds_update[NUM_LEDS];

static struct ctlra_dev_t *avtka_ui;

#define NUM_SLIDERS 8
static float sliders[NUM_SLIDERS];

#define MAX_GRID_SIZE 64
struct grid_square_t {
	uint8_t pressed;
	uint8_t pressure;
	uint16_t fb_id;
	uint32_t colour;
};
/* TODO: support more than 1 grid */
static struct grid_square_t grid[MAX_GRID_SIZE];

struct ctlra_dev_t * ctlra_avtka_connect(ctlra_event_func event_func,
					 void *userdata, void *future);
uint32_t avtka_poll(struct ctlra_dev_t *base);
int32_t avtka_disconnect(struct ctlra_dev_t *base);

/* fixme: forward decls of the functions that should be public */
void avtka_light_set(struct ctlra_dev_t *dev, uint32_t light_id,
		     uint32_t light_status);
void avtka_mirror_hw_cb(struct ctlra_dev_t* base, uint32_t num_events,
			struct ctlra_event_t** events, void *userdata);

#define LIGHT_SET(light, status)					\
	do {								\
		ctlra_dev_light_set(dev, light, status);		\
		ctlra_dev_light_set(avtka_ui, light, status);		\
	} while (0)

void simple_feedback_func(struct ctlra_dev_t *dev, void *d)
{
	struct ctlra_dev_info_t info;
	ctlra_dev_get_info(dev, &info);

	/* raw LED API *MUST* be used first */
	for(int i = 0; i < NUM_LEDS; i++) {
		if(leds_update[i]) {
			LIGHT_SET(i, leds[i]);
			leds_update[i] = 0;
		}
	}

	/* raw LED API for Grid */
	if(info.control_count[CTLRA_EVENT_GRID]) {
		for(int i = 0; i < 16; i++) {
			int id = info.grid_info[0].info.params[0] + i;
			uint32_t col = grid[i].colour * (grid[i].pressed > 0);
			ctlra_dev_light_set(dev, id, col);
		}
	}

	/* API for feedback used second - overwrites raw API */
	for(int i = 0; i < NUM_SLIDERS; i++)
		ctlra_dev_feedback_set(dev, i, sliders[i]);

	/* finally flush output */
	ctlra_dev_light_flush(dev, 1);
}

void simple_event_func(struct ctlra_dev_t* dev, uint32_t num_events,
		       struct ctlra_event_t** events, void *userdata)
{
	static const char* grid_pressed[] = { "   ", " X " };
	struct ctlra_dev_info_t info;
	ctlra_dev_get_info(dev, &info);

	avtka_mirror_hw_cb(avtka_ui, num_events, events, 0x0);

	for(uint32_t i = 0; i < num_events; i++) {
		struct ctlra_event_t *e = events[i];
		int id = 0;
		const char *pressed = 0;
		const char *name = 0;
		switch(e->type) {
		case CTLRA_EVENT_BUTTON:
			name = ctlra_info_get_name(&info, CTLRA_EVENT_BUTTON,
						   e->button.id);
			id = e->button.id;
			printf("[%s] button %s (%d)\n",
			       e->button.pressed ? " X " : "   ",
			       name, id);
			struct ctlra_item_info_t *item = &info.control_info[CTLRA_EVENT_BUTTON][id];
			if(item && item->flags & CTLRA_ITEM_HAS_FB_ID) {
				uint32_t col = item->flags & CTLRA_ITEM_LED_COLOR ?
						item->colour : 0xff000000;
				leds[item->fb_id] = e->button.pressed * col;
				leds_update[item->fb_id] = 1;
				//printf("btn id = %d, fb id = %d, col %08x\n", id, item->fb_id, col);
			}
			break;

		case CTLRA_EVENT_ENCODER:
			name = ctlra_info_get_name(&info, CTLRA_EVENT_ENCODER,
						   e->encoder.id);
			printf("[%s] encoder %s (%d)\n",
			       e->encoder.delta > 0 ? " ->" : "<- ",
			       name, e->encoder.id);
			break;

		case CTLRA_EVENT_SLIDER:
			name = ctlra_info_get_name(&info, CTLRA_EVENT_SLIDER,
						   e->slider.id);
			printf("[%03d] slider %s (%d)\n",
			       (int)(e->slider.value * 100.f),
			       name, e->slider.id);
			if(e->slider.id < NUM_SLIDERS)
				sliders[e->slider.id] = e->slider.value;
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

			id = e->grid.pos;
			grid[id].colour = 0xffffffff;
			grid[id].pressed = e->grid.pressed;
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
	struct ctlra_dev_info_t info;
	ctlra_dev_get_info(dev, &info);
	printf("simple: removing %s %s\n", info.vendor, info.device);
	if(avtka_ui) {
		/* cleanup and exit UI, resetting for next plugin */
		avtka_disconnect(avtka_ui);
		avtka_ui = 0;
	}

}

int accept_dev_func(struct ctlra_t *ctlra,
		    const struct ctlra_dev_info_t *info,
		    struct ctlra_dev_t *dev,
                    void *userdata)
{
	printf("simple: accepting %s %s\n", info->vendor, info->device);

	led_count = info->control_count[CTLRA_FEEDBACK_ITEM];
	if(led_count > NUM_LEDS)
		led_count = NUM_LEDS;

	/* dont device test a generic MIDI device - makes no sense! */
	if(strcmp(info->vendor, "OpenAV") == 0 &&
			strcmp(info->device, "Midi Generic") == 0)
		return 0;

	if(!avtka_ui) {
		avtka_ui = ctlra_avtka_connect(simple_event_func,
						 0x0, (void *)info);
		if(!avtka_ui) {
			printf("=== Critical error: avtka ui = %p.\n\
Error creating interface - please report to OpenAV. Exiting.\n", avtka_ui);
			exit(-1);
		}
		printf("oepned avtka ui\n");
	}

	ctlra_dev_set_event_func(dev, simple_event_func);
	ctlra_dev_set_feedback_func(dev, simple_feedback_func);
	ctlra_dev_set_remove_func(dev, simple_remove_func);
	ctlra_dev_set_callback_userdata(dev, userdata);

	return 1;
}

int main(int argc, char **argv)
{
	signal(SIGINT, sighndlr);

	struct ctlra_t *ctlra = ctlra_create(NULL);
	int num_devs = ctlra_probe(ctlra, accept_dev_func, ctlra);
	printf("connected devices %d\n", num_devs);

	while(!done) {
		ctlra_idle_iter(ctlra);
		if(avtka_ui)
			avtka_poll(avtka_ui);
		usleep(10 * 1000);
	}

	ctlra_exit(ctlra);

	return 0;
}
