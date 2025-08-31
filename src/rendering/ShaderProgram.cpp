#include "ShaderProgram.h"
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <sstream>
#include <cstdio>

static std::string readFile(const std::string& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) return {};
    std::ostringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

ShaderProgram::~ShaderProgram() {
    if (program) glDeleteProgram(program);
}

bool ShaderProgram::compileAttach(unsigned int type, const std::string& src, unsigned int& shaderOut) {
    shaderOut = glCreateShader(type);
    const char* csrc = src.c_str();
    glShaderSource(shaderOut, 1, &csrc, nullptr);
    glCompileShader(shaderOut);
    int success = 0; glGetShaderiv(shaderOut, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[2048]; glGetShaderInfoLog(shaderOut, 2048, nullptr, log);
        fprintf(stderr, "Shader compile error: %s\n", log);
        glDeleteShader(shaderOut);
        shaderOut = 0;
        return false;
    }
    if (!program) program = glCreateProgram();
    glAttachShader(program, shaderOut);
    return true;
}

bool ShaderProgram::loadFromFiles(const std::string& vsPath, const std::string& fsPath, const std::string& gsPath) {
    std::string vs = readFile(vsPath);
    std::string fs = readFile(fsPath);
    if (vs.empty() || fs.empty()) return false;
    unsigned int vsId=0, fsId=0, gsId=0;
    if (!compileAttach(GL_VERTEX_SHADER, vs, vsId)) return false;
    if (!compileAttach(GL_FRAGMENT_SHADER, fs, fsId)) return false;
    if (!gsPath.empty()) {
        std::string gs = readFile(gsPath);
        if (gs.empty()) return false;
        if (!compileAttach(GL_GEOMETRY_SHADER, gs, gsId)) return false;
    }
    glLinkProgram(program);
    int success=0; glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char log[2048]; glGetProgramInfoLog(program, 2048, nullptr, log);
        fprintf(stderr, "Program link error: %s\n", log);
        glDeleteProgram(program); program = 0;
        return false;
    }
    if (vsId) { glDetachShader(program, vsId); glDeleteShader(vsId);} 
    if (fsId) { glDetachShader(program, fsId); glDeleteShader(fsId);} 
    if (gsId) { glDetachShader(program, gsId); glDeleteShader(gsId);} 
    return true;
}

bool ShaderProgram::loadCompute(const std::string& csPath) {
    std::string cs = readFile(csPath);
    if (cs.empty()) return false;
    unsigned int csId=0;
    if (!compileAttach(GL_COMPUTE_SHADER, cs, csId)) return false;
    glLinkProgram(program);
    int success=0; glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char log[2048]; glGetProgramInfoLog(program, 2048, nullptr, log);
        fprintf(stderr, "Program link error: %s\n", log);
        glDeleteProgram(program); program = 0;
        return false;
    }
    if (csId) { glDetachShader(program, csId); glDeleteShader(csId);} 
    return true;
}

void ShaderProgram::use() const { glUseProgram(program); }

void ShaderProgram::setMat4(const char* name, const glm::mat4& m) const {
    int loc = glGetUniformLocation(program, name);
    glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(m));
}
void ShaderProgram::setVec3(const char* name, const glm::vec3& v) const {
    int loc = glGetUniformLocation(program, name);
    glUniform3fv(loc, 1, glm::value_ptr(v));
}
void ShaderProgram::setVec4(const char* name, const glm::vec4& v) const {
    int loc = glGetUniformLocation(program, name);
    glUniform4fv(loc, 1, glm::value_ptr(v));
}
void ShaderProgram::setFloat(const char* name, float v) const {
    int loc = glGetUniformLocation(program, name);
    glUniform1f(loc, v);
}
void ShaderProgram::setInt(const char* name, int v) const {
    int loc = glGetUniformLocation(program, name);
    glUniform1i(loc, v);
}
