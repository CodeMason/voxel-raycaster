#include "Camera.h"
#include "Pub_Sub.h"


Camera::Camera() {
}


Camera::Camera(sf::Vector3f position, sf::Vector2f direction, sf::RenderWindow* window) :
	position(position), direction(direction), window(window)
	{
		fixed = sf::Vector2i(sf::Vector2i(window->getSize().x/2, window->getSize().y/2));
}

Camera::~Camera() {
}

int Camera::set_position(sf::Vector3f position) {
	this->position = position;
	return 1;
}
	
int Camera::add_static_impulse(sf::Vector3f impulse) {
	movement += impulse;
	return 1;
}

int Camera::add_relative_impulse(DIRECTION impulse_direction, float speed) {
	
	// No sense in doing fancy dot products, adding Pi's will suffice
	// Always add PI/2 to X initially to avoid negative case
	sf::Vector2f dir;

	switch (impulse_direction) {
	
	case DIRECTION::FORWARD:
		dir = sf::Vector2f(direction.y, direction.x);
		break;
	case DIRECTION::REARWARD:
		dir = sf::Vector2f(direction.y, direction.x + PI_F);
		break;
	case DIRECTION::LEFT:
		dir = sf::Vector2f(direction.y + PI_F + PI_F / 2, PI_F / 2);
		break;
	case DIRECTION::RIGHT:
		dir = sf::Vector2f(direction.y + PI_F / 2, PI_F / 2);
		break;
	case DIRECTION::UP:
		dir = sf::Vector2f(direction.y, direction.x + PI_F / 2);
		break;
	case DIRECTION::DOWN:
		dir = sf::Vector2f(direction.y + PI_F, (direction.x * -1) + PI_F / 2 );
		break;

	}

	movement += SphereToCart(dir) * speed;

	return 1;
}

int Camera::slew_camera(sf::Vector2f input) {
	direction -= input;
	return 1;
}

void Camera::set_camera(sf::Vector2f input) {
	direction = input;
}

void Camera::set_camera(sf::Vector3f input) {
	direction = CartToNormalizedSphere(input);
}

int Camera::update(double delta_time) {
	
	double multiplier = 80;

	position.x += static_cast<float>(movement.x * delta_time * multiplier);
	position.y += static_cast<float>(movement.y * delta_time * multiplier);
	position.z += static_cast<float>(movement.z * delta_time * multiplier);
	
	movement *= static_cast<float>(friction_coefficient * delta_time * multiplier);
	
	return 1;
}

void Camera::recieve_event(VrEventPublisher* publisher, std::unique_ptr<vr::Event> event) {

	if (event->type == vr::Event::KeyHeld) {

		auto held_event = static_cast<vr::KeyHeld*>(event.get());

		if (held_event->code == sf::Keyboard::LShift) {
			default_impulse = 0.01f;
		}
		else if (held_event->code == sf::Keyboard::RShift) {
			default_impulse = 1.0f;
		}
		else if (held_event->code == sf::Keyboard::C) {
			look_at_center();
		}
		else if (held_event->code == sf::Keyboard::Q) {
			add_relative_impulse(Camera::DIRECTION::DOWN, default_impulse);
		}
		else if (held_event->code == sf::Keyboard::E) {
			add_relative_impulse(Camera::DIRECTION::UP, default_impulse);
		}
		else if (held_event->code == sf::Keyboard::W) {
			add_relative_impulse(Camera::DIRECTION::FORWARD, default_impulse);
		}
		else if (held_event->code == sf::Keyboard::S) {
			add_relative_impulse(Camera::DIRECTION::REARWARD, default_impulse);
		}
		else if (held_event->code == sf::Keyboard::A) {
			add_relative_impulse(Camera::DIRECTION::LEFT, default_impulse);
		}
		else if (held_event->code == sf::Keyboard::D) {
			add_relative_impulse(Camera::DIRECTION::RIGHT, default_impulse);
		}
		else if (held_event->code == sf::Keyboard::T) {
			set_position(sf::Vector3f(50, 50, 50));
		}
	}

	else if (event->type == vr::Event::KeyPressed) {
		
		vr::KeyPressed *key_event = static_cast<vr::KeyPressed*>(event.get());
		
		if (key_event->code == sf::Keyboard::M) {
			if (mouse_enabled)
				mouse_enabled = false;
			else
				mouse_enabled = true;
		}
	}

	else if (event->type == vr::Event::MouseMoved) {

		if (mouse_enabled) {

			vr::MouseMoved *mouse_event = static_cast<vr::MouseMoved*>(event.get());

			//deltas = fixed - sf::Mouse::getPosition();
			deltas = fixed - sf::Vector2i(mouse_event->x, mouse_event->y);
			if (deltas != sf::Vector2i(0, 0) && mouse_enabled == true) {

				sf::Mouse::setPosition(fixed, *window);
				slew_camera(sf::Vector2f(
					deltas.y / 1200.0f,
					deltas.x / 1200.0f
				));
			}
		}
	}
	else if (event->type == vr::Event::MouseButtonPressed) {

		vr::MouseButtonPressed *mouse_event = static_cast<vr::MouseButtonPressed*>(event.get());

		if (mouse_event->button == sf::Mouse::Middle) {
			mouse_enabled = !mouse_enabled;
			sf::Mouse::setPosition(fixed, *window);
		}
		
	}
	else if (event->type == vr::Event::JoystickMoved) {

		vr::JoystickMoved *joystick_event = static_cast<vr::JoystickMoved*>(event.get());

		if (joystick_event->axis == sf::Joystick::Axis::X) {
			movement.x -= joystick_event->position / 5;
			//add_relative_impulse(Camera::DIRECTION::FORWARD, joystick_event->position);
		}
		else if (joystick_event->axis == sf::Joystick::Axis::Y) {
			movement.y += joystick_event->position / 5;
			//add_relative_impulse(Camera::DIRECTION::RIGHT, joystick_event->position);
		}
		//else if (joystick_event->axis == sf::Joystick::Axis::Z) {
		//	add_relative_impulse(Camera::DIRECTION::DOWN, joystick_event->position);
		//}
	}

}

void Camera::look_at_center() {

	direction = CartToNormalizedSphere(sf::Vector3f(75, 75, 75) - position);
}

sf::Vector2f* Camera::get_direction_pointer() {
	return &direction;
}

sf::Vector3f* Camera::get_movement_pointer() {
	return &movement;
}

sf::Vector3f* Camera::get_position_pointer() {
	return &position;
}

sf::Vector3f Camera::get_movement() {
	return movement;
}

sf::Vector3f Camera::get_position() {
	return position;
}

sf::Vector2f Camera::get_direction() {
	return direction;
}

void Camera::setSpeed(float speed) {
	default_impulse = speed;;
}

float Camera::getSpeed() {
	return default_impulse;
}
