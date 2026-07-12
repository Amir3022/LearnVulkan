#include <vk_types.h>
#include <SDL.h>
#include <glm/glm.hpp>

class Camera
{
public:

    Camera();

    Camera(float cameraSpeed, float FOV, float camSensitiviy);

    glm::mat4 getViewMatrix();

    glm::mat4 getRotationMatrix();

    float getFOV() {return _FOV;}

    void updateCamera(float deltaTime);

    void processSDLEvent(const SDL_Event& e);

private:
    float _FOV;
    float _camSpeed;
    float _camSensitiviy;

    glm::vec3 _velocity;
    glm::vec3 _position;

    float _pitch, _yaw;
};
