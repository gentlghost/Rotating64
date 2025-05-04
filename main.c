#include <stdio.h>
#include <libdragon.h>
#include <t3d/t3d.h>
#include <t3d/t3dmath.h>
#include <t3d/t3dmodel.h>

#define TICKS_TO_SEC (get_ticks_ms() / 1000.0)

// Initialize all necessary components
void init(void);
// Update every frame
void update(float delta);
// Draw and render to screen
void draw(void);
// Destroy all components
void end(void);

float fclamp(float, float, float);

// Camera variables
static T3DViewport _viewport;
static const T3DVec3 _cam_position = (T3DVec3){{0.0f, 20.0f, 40.0f}};
static const T3DVec3 _target_position = (T3DVec3){{0.0f, 0.0f, 0.0f}};

// Model variables
static T3DModel *_model;
static T3DMat4 _model_matrix;
static T3DMat4FP *_matrixfp;
static rspq_block_t *_dpl_draw = NULL;

// Light variables
static T3DVec3 _light_dir = (T3DVec3){{-1.0, 1.0, 1.0}};
static uint8_t _color_ambience[4] = {0xFF, 0xFF, 0xFF, 0xFF};
static uint8_t _color_dir[4] = {0xFF, 0xFF, 0xFF, 0xFF};

int main(void)
{
    // dt (Delta Time) will be counted in seconds
    float dt, last;
    
    init();
    
    last = (float)TICKS_TO_SEC;
    while (1)
    {
        dt = (float)TICKS_TO_SEC - last;
        last = (float)TICKS_TO_SEC;
        
        update(dt);
        draw();
    }
    
    end();

    return 0;
}

void init(void)
{
    // Initialization for the controller
    joypad_init();

    // Initialization for the asset compression
    asset_init_compression(2);

    // Initialization for the Dragon File System
    dfs_init(DFS_DEFAULT_LOCATION);

    // Initialize Tiny 3D
    rdpq_init();
    t3d_init((T3DInitParams){});

    // Initialize the display
    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 3, GAMMA_NONE, FILTERS_RESAMPLE_ANTIALIAS);
    
    // Create the camera
    _viewport = t3d_viewport_create();

    // Set the model's transform matrix to its identity
    t3d_mat4_identity(&_model_matrix);
    _matrixfp = malloc_uncached(sizeof(T3DMat4FP));

    t3d_vec3_norm(&_light_dir);

    _model = t3d_model_load("rom:/n64.t3dm");
}

//static float _rot_angle = 0.0f;
static bool _paused = false;
static bool _rx = false, _ry = false, _rz = false;
static float _rotx = 0.0f, _roty = 0.0f, _rotz = 0.0f;
static float _model_scale = 0.1f;

void update(float delta)
{
    
    //_rot_angle -= 0.2f * delta;

    // Get the inputs
    joypad_poll();
    joypad_inputs_t inputs = joypad_get_inputs(JOYPAD_PORT_1);
    joypad_buttons_t btn_pressed = joypad_get_buttons_pressed(JOYPAD_PORT_1);

    // Set the rotation
    _paused = (btn_pressed.start) ? !_paused : _paused;
    if (_paused)
    {
        _rx = (inputs.btn.c_up != 0) ? true : false;
        _ry = (inputs.btn.c_left != 0) ? true : false;
        _rz = (inputs.btn.c_right != 0) ? true : false;
        
        _rotx = (_rx) ? _rotx - (2.0f * delta) : _rotx;
        _roty = (_ry) ? _roty - (2.0f * delta) : _roty;
        _rotz = (_rz) ? _rotz - (2.0f * delta) : _rotz;

        if (inputs.btn.r)
            _model_scale += 0.05f * delta;
        
        if (inputs.btn.z)
            _model_scale -= 0.05f * delta;
        
        _model_scale = fclamp(_model_scale, 0.01f, 2.0f);
    }
    else 
    {
        _rotx = 0.0f;
        _roty -= 0.2f * delta;
        _rotz = 0.0f;

        _model_scale = 0.1f;
    }
        
    t3d_viewport_set_projection(&_viewport, T3D_DEG_TO_RAD(85.0f), 10.0f, 150.0f);
    
    // Set the viewport to look at the target
    t3d_viewport_look_at(&_viewport, &_cam_position, &_target_position, &(T3DVec3){{0, 1, 0}});

    // Rotate the object
    t3d_mat4_from_srt_euler(
        &_model_matrix,
        (float[3]){_model_scale, _model_scale, _model_scale},      // Scale
        (float[3]){_rotx, _roty, _rotz},     // Rotation
        (float[3]){0.0f, 0.0f, 0.0f}        // Translation
    );

    t3d_mat4_to_fixed(_matrixfp, &_model_matrix);
}

void draw(void)
{
    // Attach the display and the Z-Buffer to RDPQ
    rdpq_attach(display_get(), display_get_zbuf());
    // Start drawing the frame
    t3d_frame_start();
    t3d_viewport_attach(&_viewport);

    t3d_screen_clear_color(RGBA32(0xFF, 0xFF, 0xFF, 0xFF));
    t3d_screen_clear_depth();

    // Render the light
    t3d_light_set_ambient(_color_ambience);
    t3d_light_set_directional(0, _color_dir, &_light_dir);
    t3d_light_set_count(1);

    // If DPLDraw does not exist, create it.
    if (!_dpl_draw)
    {
        rspq_block_begin();

        t3d_matrix_push(_matrixfp);
        t3d_model_draw(_model);
        t3d_matrix_pop(1);
        _dpl_draw = rspq_block_end();
    }

    rspq_block_run(_dpl_draw);
    rdpq_detach_show();
}

void end(void)
{
    t3d_destroy();
    display_close();
    dfs_close(DFS_DEFAULT_LOCATION);
    joypad_close();
}

float fclamp(float value, float min, float max)
{
    return (value > max) ? max : ((value < min) ? min : value);
}