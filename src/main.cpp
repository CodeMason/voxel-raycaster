#include <iostream>
#include <chrono>
#include <fstream>
#include <sstream>
#include <SFML/Graphics.hpp>
#include <GL/glew.h>

#ifdef linux
#include <CL/cl.h>
#include <CL/opencl.h>

#elif defined _WIN32
#include <CL/cl_gl.h>
#include <CL/cl.h>
#include <CL/opencl.h>
#include <windows.h>

#elif defined TARGET_OS_MAC
# include <OpenGL/OpenGL.h>
# include <OpenCL/opencl.h>

#endif
#include "TestPlatform.cpp"
#include "Map.h"
#include "Curses.h"
#include "util.hpp"
#include "RayCaster.h"


const int WINDOW_X = 150;
const int WINDOW_Y = 150;

std::string read_file(std::string file_name){
    std::ifstream input_file(file_name);

    if (!input_file.is_open()){
        std::cout << file_name << " could not be opened" << std::endl;
        return nullptr;
    }

    std::stringstream buf;
    buf << input_file.rdbuf();
    return buf.str();
}


int main(){

	int error = 0;

    // ===================================================================== //
    // ==== Opencl setup

	// Get the number of platforms
	cl_uint platform_count = 0;
	clGetPlatformIDs(0, nullptr, &platform_count);

	// Fetch the platforms
	std::vector<cl_platform_id> platformIds(platform_count);
	clGetPlatformIDs(platform_count, platformIds.data(), nullptr);



	// Print out this machines info
	
	std::cout << "============ Hardware info =================" << std::endl;

	for (unsigned int i = 0; i < platform_count; i++) {
		
		std::cout << "--Platform: " << i << std::endl;
		
		char platform[128];
		char version[128];

		clGetPlatformInfo(platformIds[i], CL_PLATFORM_NAME, 128, platform, NULL);
		clGetPlatformInfo(platformIds[i], CL_PLATFORM_VERSION, 128, version, NULL);

		std::cout << platform << "\n";
		std::cout << version << "\n\n";

		// get the number of devices, fetch them, choose the first one
		cl_uint deviceIdCount = 0;
		error = clGetDeviceIDs(platformIds[i], CL_DEVICE_TYPE_ALL, 0, nullptr, &deviceIdCount);

		std::vector<cl_device_id> deviceIds(deviceIdCount);

		for (int q = 0; q < deviceIdCount; q++) {

			std::cout << "++++Device " << q << std::endl;
			error = clGetDeviceIDs(platformIds[i], CL_DEVICE_TYPE_ALL, deviceIdCount, deviceIds.data(), NULL);

			clGetDeviceInfo(deviceIds[q], CL_DEVICE_NAME, 128, platform, NULL);
			clGetDeviceInfo(deviceIds[q], CL_DEVICE_VERSION, 128, version, NULL);
			
			std::cout << platform << "\n";
			std::cout << version << "\n\n";
		
		}
	}

	std::cout << "============================================" << std::endl;

	cl_uint deviceIdCount = 0;
	std::vector<cl_device_id> deviceIds;

	// Try to get a GPU first
	error = clGetDeviceIDs(platformIds[1], CL_DEVICE_TYPE_GPU, 0, nullptr,
		&deviceIdCount);


    if (deviceIdCount == 0) {
        std::cout << "couldn't acquire a GPU, falling back to CPU" << std::endl;
        error = clGetDeviceIDs(platformIds[1], CL_DEVICE_TYPE_CPU, 0, nullptr, &deviceIdCount);
        deviceIds.resize(deviceIdCount);
        error = clGetDeviceIDs(platformIds[1], CL_DEVICE_TYPE_CPU, deviceIdCount, deviceIds.data(), NULL);
    } else {
        std::cout << "acquired GPU cl target" << std::endl;
        deviceIds.resize(deviceIdCount);
        clGetDeviceIDs (platformIds[1], CL_DEVICE_TYPE_GPU, deviceIdCount, deviceIds.data (), nullptr);
    }



    if (error != 0){
        std::cout << "Err: clGetDeviceIDs returned: " << error << std::endl;
        return error;
    }

    // Hurray for standards!
    // Setup the context properties to grab the current GL context
    #ifdef linux
    cl_context_properties context_properties[] = {
        CL_GL_CONTEXT_KHR, (cl_context_properties) glXGetCurrentContext(),
        CL_GLX_DISPLAY_KHR, (cl_context_properties) glXGetCurrentDisplay(),
        CL_CONTEXT_PLATFORM, (cl_context_properties) platform,
        0
    };

    #elif defined _WIN32
    //cl_context_properties context_properties[] = {
    //    CL_CONTEXT_PLATFORM, (cl_context_properties) platformIds[0],
    //    CL_GL_CONTEXT_KHR, (cl_context_properties) wglGetCurrentContext(),
    //    CL_WGL_HDC_KHR, (cl_context_properties) wglGetCurrentDC(),
    //    0
    //};
	HGLRC hGLRC = wglGetCurrentContext();
	HDC hDC = wglGetCurrentDC();
	cl_context_properties context_properties[] = { CL_CONTEXT_PLATFORM, (cl_context_properties)platformIds[1], CL_GL_CONTEXT_KHR, (cl_context_properties)hGLRC, CL_WGL_HDC_KHR, (cl_context_properties)hDC, 0 };


    #elif defined TARGET_OS_MAC
    CGLContextObj glContext = CGLGetCurrentContext();
    CGLShareGroupObj shareGroup = CGLGetShareGroup(glContext);
    cl_context_properties context_properties[] = {
            CL_CONTEXT_PROPERTY_USE_CGL_SHAREGROUP_APPLE,
            (cl_context_properties)shareGroup,
            0
    };

    #endif

    // Create our shared context
	auto context = clCreateContext(
			context_properties,
			1,
			&deviceIds[0],
			nullptr, nullptr,
			&error
	);

	
	//cl_device_id devices[32]; 
	//size_t size;
	//clGetGLContextInfoKHR(context_properties, CL_DEVICES_FOR_GL_CONTEXT_KHR, 
	//	32 * sizeof(cl_device_id), devices, &size);

    if (error != 0){
        std::cout << "Err: clCreateContext returned: " << error << std::endl;
        return error;
    }

    // And the cl command queue
    auto commandQueue = clCreateCommandQueue(context, deviceIds[0], 0, &error);

    if (error != 0){
        std::cout << "Err: clCreateCommandQueue returned: " << error << std::endl;
        return error;
    }

    // At this point the shared GL/CL context is up and running
    // ====================================================================== //
    // ========== Kernel setup & compilation

    // Load in the kernel, and c stringify it
    std::string kernel_source;
    kernel_source = read_file("../kernels/kernel.txt");
    const char* kernel_source_c_str = kernel_source.c_str();
    size_t kernel_source_size = strlen(kernel_source_c_str);


    // Load the source into CL's data structure
    cl_program kernel_program = clCreateProgramWithSource(
            context, 1,
            &kernel_source_c_str,
            &kernel_source_size, &error
    );

    if (error != 0){
        std::cout << "Err: clCreateProgramWithSource returned: " << error << std::endl;
        return error;
    }


    // Try and build the program
    error = clBuildProgram(kernel_program, 1, &deviceIds[0], NULL, NULL, NULL);

    // Check to see if it errored out
    if (error == CL_BUILD_PROGRAM_FAILURE){

        // Get the size of the queued log
        size_t log_size;
        clGetProgramBuildInfo(kernel_program, deviceIds[0], CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
        char *log = new char[log_size];

        // Grab the log
        clGetProgramBuildInfo(kernel_program, deviceIds[0], CL_PROGRAM_BUILD_LOG, log_size, log, NULL);


        std::cout << "Err: clBuildProgram returned: " << error << std::endl;
        std::cout << log << std::endl;
        return error;
    }

    // Done initializing the kernel
    cl_kernel finished_kernel = clCreateKernel(kernel_program, "kernel_name", &error);

    if (error != 0){
        std::cout << "Err: clCreateKernel returned: " << error << std::endl;
        return error;
    }

};






float elap_time(){
	static std::chrono::time_point<std::chrono::system_clock> start;
	static bool started = false;

	if (!started){
		start = std::chrono::system_clock::now();
		started = true;
	}

	std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
	std::chrono::duration<double> elapsed_time = now - start;
	return elapsed_time.count();
}

sf::Sprite window_sprite;
sf::Texture window_texture;

// Y: -1.57 is straight up
// Y: 1.57 is straight down

void test_ray_reflection(){

    sf::Vector3f r(0.588, -0.78, -0.196);
    sf::Vector3f i(0, 0.928, 0.37);

    // is this needed? free spin but bounded 0 < z < pi
    if (i.z > PI)
        i.z -= PI;
    else if (i.z < 0)
        i.z += PI;

    std::cout << AngleBetweenVectors(r, i);



    return;
}

int main0() {

    // Initialize the render window
	Curses curse(sf::Vector2i(5, 5), sf::Vector2i(WINDOW_X, WINDOW_Y));
    sf::RenderWindow window(sf::VideoMode(WINDOW_X, WINDOW_Y), "SFML");
	


    // The step size in milliseconds between calls to Update()
    // Lets set it to 16.6 milliseonds (60FPS)
    float step_size = 0.0166f;

    // Timekeeping values for the loop
    double  frame_time = 0.0,
            elapsed_time = 0.0,
            delta_time = 0.0,
            accumulator_time = 0.0,
            current_time = 0.0;

    fps_counter fps;

	// ============================= RAYCASTER SETUP ==================================
	
	// Setup the sprite and texture
	window_texture.create(WINDOW_X, WINDOW_Y);
	window_sprite.setPosition(0, 0);

	// State values
	sf::Vector3i map_dim(100, 100, 100);
	sf::Vector2i view_res(WINDOW_X, WINDOW_Y);
	sf::Vector3f cam_dir(1.0f, 0.0f, 1.57f);
	sf::Vector3f cam_pos(50, 50, 50);
	sf::Vector3f cam_vec(0, 0, 0);
	Map* map = new Map(map_dim);
	RayCaster ray_caster(map, map_dim, view_res);


	// ===============================================================================

	// Mouse capture
	sf::Vector2i deltas;
	sf::Vector2i fixed(window.getSize());
	bool mouse_enabled = true;

	while (window.isOpen()) {

		// Poll for events from the user
		sf::Event event;
		while (window.pollEvent(event)) {

			// If the user tries to exit the application via the GUI
			if (event.type == sf::Event::Closed)
				window.close();
		}

		cam_vec.x = 0;
		cam_vec.y = 0;
		cam_vec.z = 0;

		if (sf::Keyboard::isKeyPressed(sf::Keyboard::Q)) {
			cam_vec.z = 1;
		}
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::E)) {
			cam_vec.z = -1;
		}
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::W)) {
			cam_vec.y = 1;
		}
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::S)) {
			cam_vec.y = -1;
		}
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::A)) {
			cam_vec.x = 1;
		}
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::D)) {
			cam_vec.x = -1;
		}
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Left)) {
            cam_dir.z = -0.1f;
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Right)) {
            cam_vec.z = +0.1f;
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Down)) {
            cam_vec.y = +0.1f;
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Up)) {
            cam_vec.y = -0.1f;
        }

		deltas = fixed - sf::Mouse::getPosition();
		if (deltas != sf::Vector2i(0, 0) && mouse_enabled == true) {

			// Mouse movement
			sf::Mouse::setPosition(fixed);
			cam_dir.y -= deltas.y / 300.0f;
			cam_dir.z -= deltas.x / 300.0f;
		}

		cam_pos.x += cam_vec.x / 1.0;
		cam_pos.y += cam_vec.y / 1.0;
		cam_pos.z += cam_vec.z / 1.0;

//		if (cam_vec.x > 0.0f)
//			cam_vec.x -= 0.1;
//		else if (cam_vec.x < 0.0f)
//			cam_vec.x += 0.1;
//
//		if (cam_vec.y > 0.0f)
//			cam_vec.y -= 0.1;
//		else if (cam_vec.y < 0.0f)
//			cam_vec.y += 0.1;
//
//		if (cam_vec.z > 0.0f)
//			cam_vec.z -= 0.1;
//		else if (cam_vec.z < 0.0f)
//			cam_vec.z += 0.1;

		std::cout << cam_vec.x << " : " << cam_vec.y << " : " << cam_vec.z << std::endl;


        // Time keeping
		elapsed_time = elap_time();
		delta_time = elapsed_time - current_time;
		current_time = elapsed_time;
		if (delta_time > 0.2f)
			delta_time = 0.2f;
		accumulator_time += delta_time;
		while ((accumulator_time - step_size) >= step_size) {
            accumulator_time -= step_size;


            // Update cycle
            curse.Update(delta_time);


        }

        // Fps cycle
        // map->moveLight(sf::Vector2f(0.3, 0));
		
		window.clear(sf::Color::Black);

		// Cast the rays and get the image
		sf::Color* pixel_colors = ray_caster.CastRays(cam_dir, cam_pos);

		for (int i = 0; i < WINDOW_X * WINDOW_Y; i++) {
			
			Curses::Tile t(sf::Vector2i(i % WINDOW_X, i / WINDOW_X));
			Curses::Slot s(L'\u0045', pixel_colors[i], sf::Color::Black);
			t.push_back(s);
			curse.setTile(t);

		}


        // Cast it to an array of Uint8's
		auto out = (sf::Uint8*)pixel_colors;

        window_texture.update(out);
		window_sprite.setTexture(window_texture);
		window.draw(window_sprite);


		curse.Render();

		// Give the frame counter the frame time and draw the average frame time
		fps.frame(delta_time);
		fps.draw(&window);

		window.display();



	}
	return 0;

}
