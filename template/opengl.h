
#pragma once

class GLTexture
{
public:
	enum { DEFAULT = 0, FLOAT = 1, INTTARGET = 2 };
	GLTexture( uint width, uint height, uint type = DEFAULT );
	~GLTexture();
	void Bind( const uint slot = 0 );
	void CopyFrom( Tmpl8::Surface* src );
	void CopyTo( Tmpl8::Surface* dst );
public:
	GLuint ID = 0;
	uint width = 0, height = 0;
};

GLTexture* GetRenderTarget();
bool WindowHasFocus();

class Shader
{
public:
	Shader( const char* vfile, const char* pfile, bool fromString );
	~Shader();
	void Init( const char* vfile, const char* pfile );
	void Compile( const char* vtext, const char* ftext );
	void Bind();
	void SetInputTexture( uint slot, const char* name, GLTexture* texture );
	void SetInputMatrix( const char* name, const mat4& matrix );
	void SetFloat( const char* name, const float v );
	void SetInt( const char* name, const int v );
	void SetUInt( const char* name, const uint v );
	void Unbind();
private:
	uint vertex = 0;
	uint pixel = 0;
	uint ID = 0;
};

#define CheckGL() { _CheckGL( __FILE__, __LINE__ ); }

void _CheckGL( const char* f, int l );
GLuint CreateVBO( const GLfloat* data, const uint size );
void BindVBO( const uint idx, const uint N, const GLuint id );
void CheckShader( GLuint shader, const char* vshader, const char* fshader );
void CheckProgram( GLuint id, const char* vshader, const char* fshader );
void DrawQuad();
