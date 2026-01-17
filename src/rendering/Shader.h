#ifndef SHADER_H
#define SHADER_H

#include <glad/glad.h>
  
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/ext/matrix_float4x4.hpp>


class Shader
{
public:
    unsigned int ID;
    std::string name;

    Shader(std::string name, const std::string& vertexPath, const std::string& fragmentPath);
    void use();

    void setBool(const std::string &name, bool value) const;
    void setInt(const std::string &name, int value) const;   
    void setFloat(const std::string &name, float value) const;
    void setMat4(const std::string &name, const glm::mat4 &value) const;
    void setMat3(const std::string &name, const glm::mat3 &value) const;
    void setVec3(const std::string &name, glm::vec3 value) const;
private:
    static void checkCompileErrors(unsigned int shader, std::string type);
};
  
#endif