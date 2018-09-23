#include "external/sokol_gfx.h"
#include "external/sokol_app.h"
#include "external/sokol_time.h"

#define STB_IMAGE_IMPLEMENTATION
#include "external/stb_image.h"

#include "game.h"

extern const char *vs_src, *fs_src;

const int SAMPLE_COUNT = 4;
sg_draw_state draw_state;

static sprite_data_t* sprite_data;
static uint64_t time;

typedef struct {
    float aspect;
} vs_params_t;

void init(void) {
    sg_setup(&(sg_desc){
        .mtl_device = sapp_metal_get_device(),
        .mtl_renderpass_descriptor_cb = sapp_metal_get_renderpass_descriptor,
        .mtl_drawable_cb = sapp_metal_get_drawable,
        .d3d11_device = sapp_d3d11_get_device(),
        .d3d11_device_context = sapp_d3d11_get_device_context(),
        .d3d11_render_target_view_cb = sapp_d3d11_get_render_target_view,
        .d3d11_depth_stencil_view_cb = sapp_d3d11_get_depth_stencil_view
    });

    /* empty, dynamic instance-data vertex buffer*/
    sg_buffer instancebuf = sg_make_buffer(&(sg_buffer_desc){
        .size = kMaxSpriteCount * sizeof(sprite_data_t),
        .usage = SG_USAGE_STREAM
    });

    /* create an index buffer for a quad */
    uint16_t indices[] = {
        0, 1, 2,  2, 1, 3,
    };
    sg_buffer ibuf = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .size = sizeof(indices),
        .content = indices,
    });

    /* create shader */
    sg_shader shd = sg_make_shader(&(sg_shader_desc) {
        .vs.uniform_blocks[0] = {
            .size = sizeof(vs_params_t)
        },
        .vs.source = vs_src,
        .fs.source = fs_src,
    });
    
    /* create an image */
    int texX, texY, texN;
    uint8_t* texData = stbi_load("data/SpaceCute-Girl4.png", &texX, &texY, &texN, 4);
    sg_image tex = sg_make_image(&(sg_image_desc){
        .width = texX,
        .height = texY,
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        .content.subimage[0][0] = { .ptr = texData, .size = texX * texY * 4 }
    });
    stbi_image_free(texData);

    /* create pipeline object */
    sg_pipeline pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .buffers[0].step_func = SG_VERTEXSTEP_PER_INSTANCE,
            .attrs = {
                [0] = { .format=SG_VERTEXFORMAT_FLOAT3 }, // instance pos + scale
                [1] = { .format=SG_VERTEXFORMAT_FLOAT4 }, // instance color
            },
        },
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .depth_stencil = {
            .depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL,
            .depth_write_enabled = true,
        },
        .rasterizer.cull_mode = SG_CULLMODE_NONE,
        .rasterizer.sample_count = SAMPLE_COUNT,
        .blend = {
            .enabled = true,
            .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
            .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
            .src_factor_alpha = SG_BLENDFACTOR_SRC_ALPHA,
            .dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
        },
    });

    /* draw state struct with resource bindings */
    draw_state = (sg_draw_state) {
        .pipeline = pip,
        .vertex_buffers[0] = instancebuf,
        .index_buffer = ibuf,
        .fs_images[0] = tex,
    };
    
    stm_setup();
    sprite_data = (sprite_data_t*)malloc(kMaxSpriteCount * sizeof(sprite_data_t));
    game_initialize();
}

void frame(void) {
    vs_params_t vs_params;
    const float w = (float) sapp_width();
    const float h = (float) sapp_height();
    vs_params.aspect = w / h;

    uint64_t dt = stm_laptime(&time);
    int sprite_count = game_update(sprite_data, stm_sec(time), (float)stm_sec(dt));
    assert(sprite_count >= 0 && sprite_count <= kMaxSpriteCount);
    sg_update_buffer(draw_state.vertex_buffers[0], sprite_data, sprite_count * sizeof(sprite_data[0]));

    sg_pass_action pass_action = {
        .colors[0] = { .action = SG_ACTION_CLEAR, .val = { 0.1f, 0.1f, 0.1f, 1.0f } }
    };
    sg_begin_default_pass(&pass_action, (int)w, (int)h);
    sg_apply_draw_state(&draw_state);
    sg_apply_uniform_block(SG_SHADERSTAGE_VS, 0, &vs_params, sizeof(vs_params));
    if (sprite_count > 0)
        sg_draw(0, 6, sprite_count);
    sg_end_pass();
    sg_commit();
}

void cleanup(void) {
    game_destroy();
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .width = 800,
        .height = 600,
        .sample_count = SAMPLE_COUNT,
        .window_title = "dod playground",
    };
}

#if defined(SOKOL_METAL)
const char* vs_src =
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "struct params_t {\n"
    "  float aspect;\n"
    "};\n"
    "struct vs_in {\n"
    "  float3 posscale [[attribute(0)]];\n"
    "  float4 color [[attribute(1)]];\n"
    "};\n"
    "struct v2f {\n"
    "  float4 color;\n"
    "  float2 uv;\n"
    "  float4 pos [[position]];\n"
    "};\n"
    "vertex v2f _main(vs_in in [[stage_in]], ushort vid [[vertex_id]], constant params_t& params [[buffer(0)]]) {\n"
    "  v2f out;\n"
    "  float x = vid / 2;\n"
    "  float y = vid & 1;\n"
    "  out.pos.x = in.posscale.x + x * in.posscale.z;\n"
    "  out.pos.y = in.posscale.y + y * in.posscale.z * params.aspect;\n"
    "  out.pos.z = 0.0f;\n"
    "  out.pos.w = 1.0f;\n"
    "  out.uv = float2(x,1-y);\n"
    "  out.color = in.color;\n"
    "  return out;\n"
    "}\n";
const char* fs_src =
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "struct v2f {\n"
    "  float4 color;\n"
    "  float2 uv;\n"
    "  float4 pos [[position]];\n"
    "};\n"
    "fragment float4 _main(v2f in [[stage_in]], texture2d<float> tex0 [[texture(0)]], sampler smp0 [[sampler(0)]]) {\n"
    "  float4 diffuse = tex0.sample(smp0, in.uv);"
    "  diffuse.rgb *= in.color.rgb;\n"
    "  return diffuse;\n"
    "}\n";
#elif defined(SOKOL_D3D11)
const char* vs_src =
    "cbuffer params: register(b0) {\n"
    "  float4x4 mvp;\n"
    "};\n"
    "struct vs_in {\n"
    "  float4 pos: POS;\n"
    "  float4 color: COLOR0;\n"
    "  uint iid: SV_InstanceID;\n"
    "};\n"
    "struct vs_out {\n"
    "  float4 color: COLOR0;\n"
    "  float4 pos: SV_Position;\n"
    "};\n"
    "vs_out main(vs_in inp) {\n"
    "  vs_out outp;\n"
    "  inp.pos.x += inp.iid * 0.1;\n"
    "  inp.pos.y += inp.iid * 0.3;\n"
    "  inp.pos.z += inp.iid * 0.4;\n"
    "  outp.pos = mul(mvp, inp.pos);\n"
    "  outp.color = inp.color;\n"
    "  return outp;\n"
    "};\n";
const char* fs_src =
    "float4 main(float4 color: COLOR0): SV_Target0 {\n"
    "  return color;\n"
    "}\n";
#else
#error Unknown graphics plaform
#endif
