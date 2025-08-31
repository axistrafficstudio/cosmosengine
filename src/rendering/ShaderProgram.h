#pragma once
#include <string>
#include <glm/glm.hpp>

class ShaderProgram {
public:
    ShaderProgram() = default;
    ~ShaderProgram();

    bool loadFromFiles(const std::string& vsPath, const std::string& fsPath, const std::string& gsPath = "");
    bool loadCompute(const std::string& csPath);
    void use() const;
    unsigned int id() const { return program; }

    // helpers
    void setMat4(const char* name, const glm::mat4& m) const;
    void setVec3(const char* name, const glm::vec3& v) const;
    void setVec4(const char* name, const glm::vec4& v) const;
    void setFloat(const char* name, float v) const;
    void setInt(const char* name, int v) const;

private:
    unsigned int program = 0;
    bool compileAttach(unsigned int type, const std::string& src, unsigned int& shaderOut);
};
