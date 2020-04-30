#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NONSTDC_NO_DEPRECATE
#include <glad/glad.h>
#include <SDL2/SDL.h>

#define TINYALLOC_IMPLEMENTATION
#include <single_file_headers/tinyalloc.h>

#define TINYTILED_ALLOC(size, ctx) TINYALLOC_ALLOC(size, ctx)
#define TINYTILED_FREE(mem, ctx) TINYALLOC_FREE(mem, ctx)
#define TINYTILED_IMPLEMENTATION
#include <single_file_headers/tinytiled.h>

#define TINYGL_IMPLEMENTATION
#include <single_file_headers/tinygl.h>

#define TINYTIME_IMPLEMENTATION
#include <single_file_headers/tinytime.h>

#define TINYPNG_IMPLEMENTATION
#include <single_file_headers/tinypng.h>

#define SPRITEBATCH_MALLOC(size, ctx) TINYALLOC_ALLOC(size, ctx)
#define SPRITEBATCH_FREE(mem, ctx) TINYALLOC_FREE(mem, ctx)
#define SPRITEBATCH_IMPLEMENTATION
#include <single_file_headers/tinyspritebatch.h>

#define STRPOOL_IMPLEMENTATION
#include <single_file_headers/strpool.h>

#define ASSETSYS_IMPLEMENTATION
#include <single_file_headers/assetsys.h>

#define TINYFILEWATCH_IMPLEMENTATION
#include <single_file_headers/tinyfilewatch.h>

#define TINYC2_IMPLEMENTATION
#include <single_file_headers/tinyc2.h>

typedef struct sprite_t sprite_t;
typedef struct shape_t shape_t;
typedef struct tile_t tile_t;

filewatch_t* filewatch;
assetsys_t* assetsys;
spritebatch_t sb;
tgContext* ctx_tg;
SDL_Window* window;
SDL_GLContext ctx_gl;
tgShader sprite_shader;
tgRenderable sprite_renderable;
float projection[16];

void* quote_image;
void* images[16 * 171];
tile_t* tiles;
int tile_count;
shape_t* shapes;
int shape_count;
int shape_ids[16 * 171];

typedef struct vertex_t
{
	float x, y;
	float u, v;
} vertex_t;

#define SPRITE_VERTS_MAX (1024 * 10)
int sprite_verts_count;
vertex_t sprite_verts[SPRITE_VERTS_MAX];

struct sprite_t
{
	SPRITEBATCH_U64 image_id;
	int depth;
	float x, y;
	float sx, sy;
	float c, s;
};

struct shape_t
{
	union
	{
		c2Circle circle;
		c2AABB aabb;
		c2Capsule capsule;
		c2Poly poly;
	} u;

	C2_TYPE type;
};

struct tile_t
{
	int x, y;
	sprite_t sprite;
	int shape_id;
};

// callbacks for tinyspritebatch.h
void batch_report(spritebatch_sprite_t* sprites, int count)
{
	// build the draw call
	tgDrawCall call;
	call.r = &sprite_renderable;
	call.textures[0] = (uint32_t)sprites[0].texture_id;
	call.texture_count = 1;

	// set texture uniform in shader
	tgSendTexture(call.r->program, "u_sprite_texture", 0);

	// NOTE:
	// perform any additional sorting here

	// build vertex buffer of quads from all sprite transforms
	call.verts = sprite_verts + sprite_verts_count;
	call.vert_count = count * 6;
	sprite_verts_count += call.vert_count;
	assert(sprite_verts_count < SPRITE_VERTS_MAX);

	vertex_t* verts = call.verts;
	for (int i = 0; i < count; ++i)
	{
		spritebatch_sprite_t* s = sprites + i;

		typedef struct v2
		{
			float x;
			float y;
		} v2;

		v2 quad[] = {
			{ -0.5f,  0.5f },
			{  0.5f,  0.5f },
			{  0.5f, -0.5f },
			{ -0.5f, -0.5f },
		};

		for (int j = 0; j < 4; ++j)
		{
			float x = quad[j].x;
			float y = quad[j].y;

			// rotate sprite about origin
			float x0 = s->c * x - s->s * y;
			float y0 = s->s * x + s->c * y;
			x = x0;
			y = y0;

			// scale sprite about origin
			x *= s->sx;
			y *= s->sy;

			// translate sprite into the world
			x += s->x;
			y += s->y;

			quad[j].x = x;
			quad[j].y = y;
		}

		// output transformed quad into CPU buffer
		vertex_t* out_verts = verts + i * 6;

		out_verts[0].x = quad[0].x;
		out_verts[0].y = quad[0].y;
		out_verts[0].u = s->minx;
		out_verts[0].v = s->maxy;

		out_verts[1].x = quad[3].x;
		out_verts[1].y = quad[3].y;
		out_verts[1].u = s->minx;
		out_verts[1].v = s->miny;

		out_verts[2].x = quad[1].x;
		out_verts[2].y = quad[1].y;
		out_verts[2].u = s->maxx;
		out_verts[2].v = s->maxy;

		out_verts[3].x = quad[1].x;
		out_verts[3].y = quad[1].y;
		out_verts[3].u = s->maxx;
		out_verts[3].v = s->maxy;

		out_verts[4].x = quad[3].x;
		out_verts[4].y = quad[3].y;
		out_verts[4].u = s->minx;
		out_verts[4].v = s->miny;

		out_verts[5].x = quad[2].x;
		out_verts[5].y = quad[2].y;
		out_verts[5].u = s->maxx;
		out_verts[5].v = s->miny;
	}

	// submit call to tinygl (does not get flushed to screen until `tgFlush` is called)
	tgPushDrawCall(ctx_tg, call);
}

void* get_pixels(SPRITEBATCH_U64 image_id)
{
	if (image_id < 2737) return images[image_id];
	else return quote_image;
}

SPRITEBATCH_U64 generate_texture_handle(void* pixels, int w, int h)
{
	GLuint location;
	glGenTextures(1, &location);
	glBindTexture(GL_TEXTURE_2D, location);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	glBindTexture(GL_TEXTURE_2D, 0);
	return (SPRITEBATCH_U64)location;
}

void destroy_texture_handle(SPRITEBATCH_U64 texture_id)
{
	GLuint id = (GLuint)texture_id;
	glDeleteTextures(1, &id);
}

void setup_SDL_and_glad()
{
	// Setup SDL and OpenGL and a window
	SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO);

	// Request OpenGL 3.2 context.
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

	// set double buffer
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	// immediate swaps
	SDL_GL_SetSwapInterval(0);

	SDL_DisplayMode dm;
	if (SDL_GetDesktopDisplayMode(0, &dm))
	{
		SDL_Log("SDL_GetDesktopDisplayMode failed: %s", SDL_GetError());
		return;
	}

	int screen_w = 640;
	int screen_h = 480;
	int centered_x = dm.w / 2 - screen_w / 2;
	int centered_y = dm.h / 2 - screen_h / 2;
	window = SDL_CreateWindow("tinycavestory demonstration", centered_x, centered_y, screen_w, screen_h, SDL_WINDOW_OPENGL|SDL_WINDOW_ALLOW_HIGHDPI);
	ctx_gl = SDL_GL_CreateContext(window);

	gladLoadGLLoader(SDL_GL_GetProcAddress);

	int major, minor;
	SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &major);
	SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &minor);
	printf("SDL says running on OpenGL ES version %d.%d\n", major, minor);
	printf("Glad says OpenGL ES version : %d.%d\n", GLVersion.major, GLVersion.minor);
	printf("OpenGL says : ES %s, GLSL %s\n", glGetString(GL_VERSION), glGetString(GL_SHADING_LANGUAGE_VERSION));
}

void setup_tinygl()
{
	// setup tinygl
	int clear_bits = GL_COLOR_BUFFER_BIT;
	int settings_bits = 0;
	ctx_tg = tgMakeCtx(1024, clear_bits, settings_bits);

#define STR(x) #x

	const char* vs = STR(
		#version 300 es\n

		uniform mat4 u_mvp;

		in vec2 in_pos;
		in vec2 in_uv;

		out vec2 v_uv;

		void main( )
		{
			v_uv = in_uv;
			gl_Position = u_mvp * vec4(in_pos, 0, 1);
		}
	);

	const char* ps = STR(
		#version 300 es\n
		precision mediump float;
	
		uniform sampler2D u_sprite_texture;

		in vec2 v_uv;
		out vec4 out_col;

		void main()
		{
			out_col = texture(u_sprite_texture, v_uv);
		}
	);

	tgVertexData vd;
	tgMakeVertexData(&vd, 1024 * 1024, GL_TRIANGLES, sizeof(vertex_t), GL_DYNAMIC_DRAW);
	tgAddAttribute(&vd, "in_pos", 2, TG_FLOAT, TG_OFFSET_OF(vertex_t, x));
	tgAddAttribute(&vd, "in_uv", 2, TG_FLOAT, TG_OFFSET_OF(vertex_t, u));

	tgMakeRenderable(&sprite_renderable, &vd);
	tgLoadShader(&sprite_shader, vs, ps);
	tgSetShader(&sprite_renderable, &sprite_shader);
	
	tgOrtho2D((float)640, (float)480, 0, 0, projection);
	glViewport(0, 0, 640, 480);

	tgSendMatrix(&sprite_shader, "u_mvp", projection);
	tgLineMVP(ctx_tg, projection);

	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void swap_buffers()
{
	SDL_GL_SwapWindow(window);
}

#define AS_CHECK_ERROR(code) do { if (assetsys_check_code(code)) return 1; } while (0)
int assetsys_check_code(assetsys_error_t code)
{
	switch (code)
	{
		case ASSETSYS_ERROR_INVALID_PATH: printf("ASSETSYS_ERROR_INVALID_PATH\n"); return 1; 
		case ASSETSYS_ERROR_INVALID_MOUNT: printf("ASSETSYS_ERROR_INVALID_MOUNT\n"); return 1; 
		case ASSETSYS_ERROR_FAILED_TO_READ_ZIP: printf("ASSETSYS_ERROR_FAILED_TO_READ_ZIP\n"); return 1; 
		case ASSETSYS_ERROR_FAILED_TO_CLOSE_ZIP: printf("ASSETSYS_ERROR_FAILED_TO_CLOSE_ZIP\n"); return 1; 
		case ASSETSYS_ERROR_FAILED_TO_READ_FILE: printf("ASSETSYS_ERROR_FAILED_TO_READ_FILE\n"); return 1; 
		case ASSETSYS_ERROR_FILE_NOT_FOUND: printf("ASSETSYS_ERROR_FILE_NOT_FOUND\n"); return 1; 
		case ASSETSYS_ERROR_DIR_NOT_FOUND: printf("ASSETSYS_ERROR_DIR_NOT_FOUND\n"); return 1; 
		case ASSETSYS_ERROR_INVALID_PARAMETER: printf("ASSETSYS_ERROR_INVALID_PARAMETER\n"); return 1; 
		case ASSETSYS_ERROR_BUFFER_TOO_SMALL: printf("ASSETSYS_ERROR_BUFFER_TOO_SMALL\n"); return 1; 
	}

	return 0;
}

int load_file(const char* path, void** out, int* size)
{
	*out = 0;
	if (size) *size = 0;
	assetsys_file_t file;
	AS_CHECK_ERROR(assetsys_file(assetsys, path, &file));
	int sz = assetsys_file_size(assetsys, file);

	void* mem = TINYALLOC_ALLOC((size_t)sz, 0);
	while (1)
	{
		assetsys_error_t code = assetsys_file_load(assetsys, file, &sz, mem, sz);

		switch (code)
		{
		case ASSETSYS_ERROR_BUFFER_TOO_SMALL:
			TINYALLOC_FREE(mem, 0);
			mem = TINYALLOC_ALLOC((size_t)sz, 0);
			break;

		case ASSETSYS_SUCCESS:
			goto end;

		default:
			AS_CHECK_ERROR(code);
			// returns 1
		}
	}

	end:
	*out = mem;
	if (size) *size = sz;
	return 0;
}

// Custom function to pull all tile images out of the cavestory_tiles.png file as separate
// 15x15 images in memory, ready for tinyspritebatch requests as-needed.
void load_images()
{
	void* png_data;
	int png_length;
	if (load_file("/data/cavestory_tiles.png", &png_data, &png_length)) return;

	int border_padding = 1;
	tpImage img = tpLoadPNGMem(png_data, png_length);
	int img_count = 0;

	for (int i = 0; i < 171; ++i)
	{
		for (int j = 0; j < 16; ++j)
		{
			int horizontal_index = (border_padding + 15) * j + border_padding;
			int vertical_index = (border_padding + 15) * img.w * i + img.w * border_padding;
			tpPixel* src = img.pix + horizontal_index + vertical_index;
			tpPixel* dst = (tpPixel*)malloc(sizeof(tpPixel) * 15 * 15);

			for (int row = 0; row < 15; ++row)
			{
				tpPixel* src_row = src + row * img.w;
				tpPixel* dst_row = dst + row * 15;
				memcpy(dst_row, src_row, sizeof(tpPixel) * 15);
			}

			images[img_count++] = dst;
		}
	}

	tpFreePNG(&img);
}

void free_images()
{
	for (int i = 0; i < sizeof(images) / sizeof(images[0]); ++i)
	{
		free(images[i]);
	}
}

#define push_sprite(sp, w, h) \
	spritebatch_push(&sb, sp.image_id, w, h, sp.x, sp.y, sp.sx, sp.sy, sp.c, sp.s, sp.depth)

void scene()
{
	for (int i = 0; i < tile_count; ++i)
	{
		push_sprite(tiles[i].sprite, 15, 15);
	}
}

void init_shapes()
{
	c2AABB bb;
	bb.min = c2V(-0.5f, -0.5f);
	bb.max = c2V(0.5f, 0.5f);

	c2Poly tri_smaller;
	tri_smaller.verts[0] = c2V(-0.5f, -0.5f);
	tri_smaller.verts[1] = c2V(0.5f, -0.5f);
	tri_smaller.verts[2] = c2V(0.5f, 0);
	tri_smaller.count = 3;
	c2Norms(tri_smaller.verts, tri_smaller.norms, 3);

	c2Poly tri_larger;
	tri_larger.verts[0] = c2V(-0.5f, -0.5f);
	tri_larger.verts[1] = c2V(0.5f, -0.5f);
	tri_larger.verts[2] = c2V(0.5f, 0.5f);
	tri_larger.verts[3] = c2V(-0.5f, 0);
	tri_larger.count = 4;
	c2Norms(tri_larger.verts, tri_larger.norms, 3);

	shape_t shape0;
	shape0.u.poly = tri_larger;
	shape0.type = C2_POLY;

	shape_t shape1;
	shape1.u.poly = tri_smaller;
	shape1.type = C2_POLY;

	shape_t shape2;
	shape2.u.aabb = bb;
	shape2.type = C2_AABB;

	shapes = (shape_t*)malloc(sizeof(shape_t) * 3);
	shapes[0] = shape0;
	shapes[1] = shape1;
	shapes[2] = shape2;
	shape_count = 3;
}

tinytiled_map_t* load_map(const char* map_path)
{
	void* map_memory;
	int sz;
	if (load_file(map_path, &map_memory, &sz)) return 0;
	tinytiled_map_t* map = tinytiled_load_map_from_memory(map_memory, sz, 0);
	return map;
}

void load_tile_shapes(const char* map_path)
{
	tinytiled_map_t* map = load_map(map_path);
	tinytiled_tileset_t* tileset = map->tilesets;
	int map_width = map->width;
	int map_height = map->height;
	init_shapes();

	// read the shape id from each tile
	tinytiled_layer_t* layer = map->layers;
	int* tile_ids = layer->data;
	shape_count = layer->data_count;
	for (int i = 0; i < layer->data_count; ++i)
	{
		// grab and clear the flipping bits
		int hflip, vflip, dflip;
		int global_tile_id = tile_ids[i];
		tinytiled_get_flags(global_tile_id, &hflip, &vflip, &dflip);
		global_tile_id = tinytiled_unset_flags(global_tile_id);

		// assume the map file only has one tileset
		// resolve the tile id based on the matching tileset's first gid
		// the final index can be used on the `images` global array
		int id = global_tile_id - tileset->firstgid;

		// -1 since the first shape is "empty", or no shape at all
		shape_ids[i] = id - 1;
	}

	// assumed true for this test map
	assert(layer->data_count == map_width * map_height);

	tinytiled_free_map(map);
}

void load_tile_map(const char* map_path)
{
	tinytiled_map_t* map = load_map(map_path);
	tinytiled_tileset_t* tileset = map->tilesets;
	if (!map) return;

	int map_width = map->width;
	int map_height = map->height;

	tiles = (tile_t*)malloc(sizeof(tile_t) * map_width * map_height);

	// create tiles based off of the parsed map file data
	tinytiled_layer_t* layer = map->layers;
	int* tile_ids = layer->data;
	tile_count = layer->data_count;

	// assumed true for this test map
	assert(tile_count == map_width * map_height);

	for (int i = 0; i < tile_count; ++i)
	{
		// grab and clear the flipping bits
		int hflip, vflip, dflip;
		int global_tile_id = tile_ids[i];
		tinytiled_get_flags(global_tile_id, &hflip, &vflip, &dflip);
		global_tile_id = tinytiled_unset_flags(global_tile_id);

		// assume the map file only has one tileset
		// resolve the tile id based on the matching tileset's first gid
		// the final index can be used on the `images` global array
		int id = global_tile_id - tileset->firstgid;

		int x = i % map_width;
		int y = map_height - i / map_width;

		sprite_t sprite;
		sprite.image_id = id;
		sprite.depth = 0;
		sprite.x = (float)x;
		sprite.y = (float)y;
		sprite.sx = 1.0f;
		sprite.sy = 1.0f;

		// Handle flip flags by enumerating all bit possibilities.
		// Three bits means 2^3 = 8 possible sets. This is to support the
		// flip and rotate tile buttons in Tiled.
		int flags = dflip << 0 | vflip << 1 | hflip << 2;

		// just hard-code the sin/cos values directly since they are all factors of 90 degrees
#define ROTATE_90_CCW(sp) do { sp.c = 0.0f; sp.s = 1.0f; } while (0)
#define ROTATE_90_CW(sp) do { sp.c = 0.0f; sp.s = -1.0f; } while (0)
#define ROTATE_180(sp) do { sp.c = -1.0f; sp.s = 0.0f; } while (0)
#define FLIP_VERTICAL(sp) sp.sy *= -1.0f
#define FLIP_HORIZONTAL(sp) sp.sx *= -1.0f

		sprite.c = 1.0f;
		sprite.s = 0.0f;

		// [hflip, vflip, dflip]
		switch (flags)
		{
		case 1: // 001
			ROTATE_90_CCW(sprite);
			FLIP_VERTICAL(sprite);
			break;

		case 2: // 010
			FLIP_VERTICAL(sprite);
			break;

		case 3: // 011
			ROTATE_90_CCW(sprite);
			break;

		case 4: // 100
			FLIP_HORIZONTAL(sprite);
			break;

		case 5: // 101
			ROTATE_90_CW(sprite);
			break;

		case 6: // 110
			ROTATE_180(sprite);
			break;

		case 7: // 111
			ROTATE_90_CCW(sprite);
			FLIP_HORIZONTAL(sprite);
			break;
		}

		// draw tiles from bottom left corner of each tile
		sprite.x += 0.5f;
		sprite.y -= 0.5f;

		// center all tiles onto the screen
		sprite.x -= (float)(map_width / 2);
		sprite.y -= (float)(map_height / 2);

		// scale by tile size
		sprite.x *= 15;
		sprite.y *= 15;
		sprite.sx *= 15;
		sprite.sy *= 15;

		// scale by factor of two (source pixels are 2x2 pixel blocks on screen)
		sprite.x *= 2.0f;
		sprite.y *= 2.0f;
		sprite.sx *= 2.0f;
		sprite.sy *= 2.0f;

		// construct the tile sprite and (x, y) position
		tile_t tile;
		tile.sprite = sprite;
		tile.x = x;
		tile.y = y;
		tile.shape_id = shape_ids[id];

		tiles[i] = tile;
	}

	tinytiled_free_map(map);
}

void reload_map(filewatch_update_t change, const char* virtual_path, void* udata)
{
	(void)udata;
	if (change != FILEWATCH_FILE_MODIFIED) return;

	if (!strcmp(virtual_path, "/data/cavestory_tiles.json"))
	{
		free(tiles);
		load_tile_map("/data/cavestory_tiles.json");
	}
}

shape_t get_transformed_tile_shape(tile_t tile, shape_t shape)
{
	switch (shape.type)
	{
		case C2_POLY:
		{
			c2Poly poly = shape.u.poly;
			for (int j = 0; j < poly.count; ++j)
			{
				c2v v = c2V(poly.verts[j].x * tile.sprite.sx, poly.verts[j].y * tile.sprite.sy);
				poly.verts[j] = c2V(v.x + tile.sprite.x, v.y + tile.sprite.y);
			}
			shape.u.poly = poly;
		}	break;

		case C2_AABB:
		{
			c2v a = shape.u.aabb.min;
			c2v b = shape.u.aabb.max;
			a = c2V(a.x * c2Abs(tile.sprite.sx) + tile.sprite.x, a.y * c2Abs(tile.sprite.sy) + tile.sprite.y);
			b = c2V(b.x * c2Abs(tile.sprite.sx) + tile.sprite.x, b.y * c2Abs(tile.sprite.sy) + tile.sprite.y);
			shape.u.aabb.min = a;
			shape.u.aabb.max = b;
		}	break;
	}
	return shape;
}

void debug_draw_tile_shapes()
{
	tgLineColor(ctx_tg, 1, 0, 0);
	for (int i = 0; i < tile_count; ++i)
	{
		tile_t tile = tiles[i];
		if (tile.shape_id == -1) continue;
		shape_t shape = shapes[tile.shape_id];
		shape = get_transformed_tile_shape(tile, shape);

		switch (shape.type)
		{
		case C2_POLY:
		{
			c2Poly poly = shape.u.poly;
			for (int j = 0; j < poly.count; ++j)
			{
				int iA = j;
				int iB = (j + 1) % poly.count;
				c2v a = poly.verts[iA];
				c2v b = poly.verts[iB];
				tgLine(ctx_tg, a.x, a.y, 0, b.x, b.y, 0);
			}
		}	break;

		case C2_AABB:
		{
			c2v a = shape.u.aabb.min;
			c2v b = shape.u.aabb.max;
			c2v c = c2V(a.x, b.y);
			c2v d = c2V(b.x, a.y);
			tgLine(ctx_tg, a.x, a.y, 0, c.x, c.y, 0);
			tgLine(ctx_tg, c.x, c.y, 0, b.x, b.y, 0);
			tgLine(ctx_tg, b.x, b.y, 0, d.x, d.y, 0);
			tgLine(ctx_tg, d.x, d.y, 0, a.x, a.y, 0);
		}	break;
		}
	}
}

int main(int argc, char** argv)
{
	TINYTILED_UNUSED(argc);
	TINYTILED_UNUSED(argv);

	// setup assetsys and filewatch
	assetsys = assetsys_create(0);
	filewatch = filewatch_create(assetsys, 0);
	filewatch_mount(filewatch, "./assets", "/data");
	filewatch_start_watching(filewatch, "/data", reload_map, 0);

	setup_SDL_and_glad();
	setup_tinygl();
	load_images();

	// load the collision shape for each tile type
	load_tile_shapes("/data/collision.json");

	// load up the tile image ids, positions, and transforms
	load_tile_map("/data/cavestory_tiles.json");

	// setup the sprite batcher
	spritebatch_config_t config;
	spritebatch_set_default_config(&config);
	config.pixel_stride = sizeof(tpPixel);
	config.lonely_buffer_count_till_flush = 1;
	config.ticks_to_decay_texture = 1;

	// Assign the four callbacks so the sprite batcher knows how to get
	// pixels whenever it needs them, and assign them GPU texture handles.
	config.batch_callback = batch_report;                       // report batches of sprites from `spritebatch_flush`
	config.get_pixels_callback = get_pixels;                    // used to retrieve image pixels from `spritebatch_flush` and `spritebatch_defrag`
	config.generate_texture_callback = generate_texture_handle; // used to generate a texture handle from `spritebatch_flush` and `spritebatch_defrag`
	config.delete_texture_callback = destroy_texture_handle;    // used to destroy a texture handle from `spritebatch_defrag`

	// initialize tinyspritebatch
	int failed = spritebatch_init(&sb, &config);
	if (failed)
	{
		printf("spritebatch_init failed due to bad configuration values, or out of memory error.");
		return -1;
	}

	void (*scenes[])() = {
		scene,
	};
	int scene = 0;

	// quote things
	void* quote_sprite_mem;
	int sz;
	if (load_file("/data/quote.png", &quote_sprite_mem, &sz)) return -1;
	tpImage quote_png = tpLoadPNGMem(quote_sprite_mem, sz);
	quote_image = quote_png.pix;
	sprite_t quote_sprite;
	quote_sprite.depth = 1;
	quote_sprite.image_id = 2737 + 1;
	quote_sprite.c = 1;
	quote_sprite.s = 0;
	quote_sprite.sx = (float)quote_png.w * 2.0f;
	quote_sprite.sy = (float)quote_png.h * 2.0f;
	quote_sprite.x = 0;
	quote_sprite.y = 0;

	c2Circle quote_circle;
	quote_circle.r = quote_sprite.sy / 2.0f;
	float quote_x = 0;
	float quote_y = 0;
	float quote_vel_x = 0;
	float quote_vel_y = 0;
	int debug_draw_shapes = 1;
	int dir = 0;

	// game main loop
	int application_running = 1;
	float accum = 0;
	while (application_running)
	{
		float dt = ttTime();

		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
			case SDL_QUIT:
				application_running = 0;
				break;

			case SDL_KEYDOWN:
			{
				SDL_Keycode key = event.key.keysym.sym;
				if (key == SDLK_1) debug_draw_shapes = !debug_draw_shapes;;
				if (key == SDLK_SPACE) quote_vel_y = 250.0f;
				if (key == SDLK_a) dir = 1;
				else if (key == SDLK_d) dir = 2;
			}	break;

			case SDL_KEYUP:
			{
				SDL_Keycode key = event.key.keysym.sym;
				if (key == SDLK_a) dir = 0;
				else if (key == SDLK_d) dir = 0;
			}	break;
			}
		}

		switch (dir)
		{
		case 0: quote_vel_x = 0; break;
		case 1: quote_vel_x = -100.0f; break;
		case 2: quote_vel_x = 100.0f; break;
		}

		// render 60fps
		accum += dt;
		if (accum < (1.0f / 60.0f)) continue;
		dt = accum;
		accum = 0;

		// support asset hotloading
		filewatch_update(filewatch);
		filewatch_notify(filewatch);

		// quote's physics integration
		quote_vel_y += dt * -250.0f;
		quote_x += dt * quote_vel_x;
		quote_y += dt * quote_vel_y;
		quote_circle.p = c2V(quote_x, quote_y);
		quote_sprite.x = quote_x;
		quote_sprite.y = quote_y;

		// quote's collision detection and collision resolution
		for (int i = 0; i < tile_count; ++i)
		{
			tile_t tile = tiles[i];
			if (tile.shape_id == -1) continue;
			shape_t shape = shapes[tile.shape_id];
			shape = get_transformed_tile_shape(tile, shape);
			c2Manifold m;
			c2Collide(&quote_circle, 0, C2_CIRCLE, &shape.u, 0, shape.type, &m);
			if (m.count)
			{
				// move quote out of colliding configuration
				float depth = -m.depths[0];
				c2v n = m.n;
				quote_x += n.x * depth;
				quote_y += n.y * depth;

				// "slide along wall"
				// remove all velocity in the direction of the wall's normal
				// v -= n * dot(v, n)
				c2v v = c2V(quote_vel_x, quote_vel_y);
				v = c2Sub(v, c2Mulvs(n, c2Dot(v, n)));
				quote_vel_x = v.x;
				quote_vel_y = v.y;
			}
		}

		// push some sprites to tinyspritebatch
		// rendering tiles
		scenes[scene]();

		// draw quote
		if (dir == 1) quote_sprite.sx *= c2Sign(quote_sprite.sx) < 0 ? -1.0f : 1.0f;
		if (dir == 2) quote_sprite.sx *= c2Sign(quote_sprite.sx) < 0 ? 1.0f : -1.0f;
		push_sprite(quote_sprite, quote_png.w, quote_png.h);

		// a typical usage of tinyspritebatch
		// simply defrag each frame, with one tick, and a flush
		// flush will create some tinygl draw calls
		spritebatch_tick(&sb);
		spritebatch_tick(&sb);
		spritebatch_defrag(&sb);
		spritebatch_flush(&sb);
		sprite_verts_count = 0;

		if (debug_draw_shapes)
		{
			debug_draw_tile_shapes();

			// draw quote circle shape
			c2v p = quote_circle.p;
			float r = quote_circle.r;
			int kSegs = 40;
			float theta = 0;
			float inc = 3.14159265f * 2.0f / (float)kSegs;
			float px, py;
			c2SinCos( theta, &py, &px );
			px *= r; py *= r;
			px += p.x; py += p.y;
			for (int i = 0; i <= kSegs; ++i)
			{
				theta += inc;
				float x, y;
				c2SinCos( theta, &y, &x );
				x *= r; y *= r;
				x += p.x; y += p.y;
				tgLine(ctx_tg, x, y, 0, px, py, 0);
				px = x; py = y;
			}
		}

		int calls = tgDrawCallCount(ctx_tg);
		printf("Draw call count: %d\n", calls);

		// sprite batches have been submit to tinygl, go ahead and flush to screen
		tgFlush(ctx_tg, swap_buffers, 0, 640, 480);
		TG_PRINT_GL_ERRORS();
	}

	free_images();
	tpFreePNG(&quote_png);
	spritebatch_term(&sb);
	tgFreeCtx(ctx_tg);

	TINYALLOC_CHECK_FOR_LEAKS();

	return 0;
}
