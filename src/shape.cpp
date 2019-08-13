#include "../include/shape.h"
#include <fstream>
#include <string>
#include <iostream>
#include <algorithm>
using namespace std;

#define CHECK_GL_ERROR() CheckGLError(__FILE__, __LINE__)

//list <Shape*> allShapes;

void checkGLError(int file, int line){
	GLenum err;
	while((err = glGetError()) != GL_NO_ERROR){
		cout << file << ":" << line << "Error: " << gluErrorString(err);
	}
}

void printStatus(const char *step, GLuint context, GLuint status){
	GLint result = GL_FALSE;
	glGetShaderiv(context, status, &result);
	if (result == GL_FALSE) {
		char buffer[1024];
		if (status == GL_COMPILE_STATUS)
			glGetShaderInfoLog(context, 1024, NULL, buffer);
		else
			glGetProgramInfoLog(context, 1024, NULL, buffer);
		if (buffer[0])
			fprintf(stderr, "%s: %s\n", step, buffer);
	}
}


GLuint loadShader(string filename, GLenum shader_type){
	cout << "Loading shader from " << filename << endl;
	ifstream fin(filename.c_str());
	string c((istreambuf_iterator<char>(fin)), istreambuf_iterator<char>());
	const char * glsl_src = c.c_str();

	GLuint shader_id = glCreateShader(shader_type);
	glShaderSource(shader_id, 1, &glsl_src, NULL);
	glCompileShader(shader_id);
	printStatus(filename.c_str(), shader_id, GL_COMPILE_STATUS);
	
	return shader_id;
}

//void Shape::createShaders(){


//}

list<Shape*> Shape::allShapes;

Shape::Shape(int nverts, int _dim){
	nVertices = nverts;
	dim = _dim;
	useColor = useTexture = useElements = false;
	
	vertexShader = loadShader("src/shaders/shader_vertex_tex.glsl", GL_VERTEX_SHADER);
	fragmentShader = loadShader("src/shaders/shader_fragment_tex.glsl", GL_FRAGMENT_SHADER);

	program = glCreateProgram();
	glAttachShader(program, vertexShader);
	glAttachShader(program, fragmentShader);
	glLinkProgram(program);
	printStatus("Shader program", program, GL_LINK_STATUS);
	
	glGenBuffers(1, &vbo);
	glGenBuffers(1, &cbo);
	glGenBuffers(1, &ebo);
	glGenBuffers(1, &tbo);
	
	// apply a dummy 1x1 white texture 
	unsigned char pixels[] = {255,255,255,255}; 
	glGenTextures(1, &tex);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tex);
	glUniform1i(glGetUniformLocation(program, "tex"), 0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels); // After reading one row of texels, pointer advances to next 4 byte boundary. Therefore ALWAYS use 4byte colour types. 
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glGenerateMipmap(GL_TEXTURE_2D);
	
	allShapes.push_back(this);
}

Shape::~Shape(){

	glBindTexture(GL_TEXTURE_2D, 0);
	glDeleteTextures(1, &tex);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glDeleteBuffers(1, &ebo);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glDeleteBuffers(1, &vbo);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glDeleteBuffers(1, &cbo);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glDeleteBuffers(1, &tbo);

	glDetachShader(program, vertexShader);
	glDetachShader(program, fragmentShader);
	glDeleteProgram(program);
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	try{
		list<Shape*>::iterator it = find(allShapes.begin(), allShapes.end(), this);
		if (it == allShapes.end()) throw string("Shape destructor could not find shape in global list!");
		allShapes.erase(it);
	}
	catch(string s){ 
		cout << "FATAL ERROR: " << s << endl; 
	}
	
}


void Shape::setVertices(float * verts){
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, dim*nVertices*sizeof(float), verts, GL_DYNAMIC_DRAW);
}

void Shape::setColors(float * cols){
	useColor = true;
	glBindBuffer(GL_ARRAY_BUFFER, cbo);
	glBufferData(GL_ARRAY_BUFFER, 4*nVertices*sizeof(float), cols, GL_DYNAMIC_DRAW);
}

void Shape::setElements(int * ele, int nelements){
	useElements = true;
	nElements = nelements;
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, nelements*sizeof(int), ele, GL_DYNAMIC_DRAW);
}


void Shape::applyTexture(float * uvs, unsigned char * pixels, int w, int h){
	useTexture = true;
	glBindBuffer(GL_ARRAY_BUFFER, tbo);
	glBufferData(GL_ARRAY_BUFFER, 2*nVertices*sizeof(float), uvs, GL_DYNAMIC_DRAW);

	glGenTextures(1, &tex);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tex);
	glUniform1i(glGetUniformLocation(program, "tex"), 0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels); // After reading one row of texels, pointer advances to next 4 byte boundary. Therefore ALWAYS use 4byte colour types. 
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glGenerateMipmap(GL_TEXTURE_2D);

}

void Shape::render(){

	GLuint pos_loc = glGetAttribLocation(program, "in_pos");
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glVertexAttribPointer(pos_loc, dim, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(pos_loc);

	GLuint col_loc = glGetAttribLocation(program, "in_col");
	if (useColor){
		glBindBuffer(GL_ARRAY_BUFFER, cbo);
		glVertexAttribPointer(glGetAttribLocation(program, "in_col"), 4, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(col_loc);
	}
	else{
		glVertexAttrib4f(col_loc, 1, 1, 1, 0.5);
	}
	
	GLuint uv_loc = glGetAttribLocation(program, "in_UV");
	if (useTexture){
		glBindBuffer(GL_ARRAY_BUFFER, tbo);
		glVertexAttribPointer(uv_loc, 2, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(uv_loc);
	}
	else{
		glVertexAttrib2f(glGetAttribLocation(program, "in_UV"), 0, 0);
	}
	
	glUseProgram(program);
	glDrawElements(GL_TRIANGLES, nElements, GL_UNSIGNED_INT, (void *)0);

	if (useTexture) glDisableVertexAttribArray(uv_loc);
	if (useColor)   glDisableVertexAttribArray(col_loc);
	glDisableVertexAttribArray(pos_loc);

}


//void Shape::deleteTexture(){
//	if (useTexture){
//	}
//}



