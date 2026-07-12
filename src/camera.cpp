#include "camera.h"

//Headers from GLM
#include <glm/gtx/transform.hpp> 
#include <glm/gtx/quaternion.hpp>
#include <glm/mat4x4.hpp>

Camera::Camera()
{
    _velocity = glm::vec3(0.0f);
    _position = glm::vec3(0.0f);
    _pitch = _yaw = 0.0f;

    _FOV = 45.0f;
    _camSpeed = 5.0f;
    _camSensitiviy = 0.1f;
}

Camera::Camera(float cameraSpeed, float FOV, float camSensitiviy) : Camera()
{
    _FOV = FOV;
    _camSpeed = cameraSpeed;
    _camSensitiviy = camSensitiviy;
}

glm::mat4 Camera::getViewMatrix()
{
    //View matrix is the negation of camera position, since camera movement in a direction will view all rendered vertices in negated direction
    glm::mat4 cameraPosMat =  glm::translate(glm::identity<glm::mat4>(), _position);
    return glm::inverse(cameraPosMat * getRotationMatrix());
}

glm::mat4 Camera::getRotationMatrix()
{
    glm::quat yawQuat = glm::angleAxis(glm::radians(_yaw), glm::vec3(0.0f, -1.0, 0.0f));
    glm::quat pitchQuat = glm::angleAxis(glm::radians(_pitch), glm::vec3(1.0f, 0.0, 0.0f));

    return glm::toMat4(yawQuat) * glm::toMat4(pitchQuat);
}

void Camera::updateCamera(float deltaTime)
{
    //Update the position with velocity in the camera rotation moved by deltatime
    _position += glm::vec3(getRotationMatrix() * glm::vec4(_velocity * deltaTime * _camSpeed, 0.0f));
}

void Camera::processSDLEvent(const SDL_Event& e)
{
    if(e.type == SDL_KEYDOWN)
    {
        if(e.key.keysym.sym == SDLK_w)
            _velocity.z = -1;
        else if(e.key.keysym.sym == SDLK_s)
            _velocity.z = 1;
        if(e.key.keysym.sym == SDLK_d)
            _velocity.x = 1;
        else if(e.key.keysym.sym == SDLK_a)
            _velocity.x = -1;
    }
    else if(e.type == SDL_KEYUP)
    {
        if(e.key.keysym.sym == SDLK_w)
            _velocity.z = 0;
        if(e.key.keysym.sym == SDLK_s)
            _velocity.z = 0;
        if(e.key.keysym.sym == SDLK_d)
            _velocity.x = 0;
        if(e.key.keysym.sym == SDLK_a)
            _velocity.x = 0;
    }

    //Normalize velocity so we don't end with higher speed when moving diagonal
    if(glm::length(_velocity) > 0)
        _velocity = glm::normalize(_velocity);

    //Get Relative difference in mouse position to detect mouse movement and use it to rotate camera
    if(e.type == SDL_MOUSEMOTION)
    {
        _pitch -= (float)e.motion.yrel * _camSensitiviy;
        _yaw += (float)e.motion.xrel * _camSensitiviy;
    }
}
