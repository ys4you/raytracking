
#include <chrono>
#include <fstream>
#include <vector>
#include <list>
#include <string>
#include <math.h>
#include <algorithm>
#include <assert.h>
#include <io.h>

#include <immintrin.h>

typedef unsigned char uchar;
typedef unsigned int uint;
typedef unsigned short ushort;
typedef unsigned short half;
#ifdef _MSC_VER
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned long DWORD;
typedef int BOOL;
#endif

using namespace std;


#define NOMINMAX
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#endif
#define NOGDICAPMASKS
#define NOWINMESSAGES
#define NOWINSTYLES
#define NOSYSMETRICS
#define NOMENUS
#define NOICONS
#define NOKEYSTATES
#define NOSYSCOMMANDS
#define NORASTEROPS
#define NOSHOWWINDOW
#define OEMRESOURCE
#define NOATOM
#define NOCLIPBOARD
#define NOCOLOR
#define NOCTLMGR
#define NODRAWTEXT
#define NOKERNEL
#define NONLS
#define NOMEMMGR
#define NOMETAFILE
#define NOMINMAX
#define NOMSG
#define NOOPENFILE
#define NOSCROLL
#define NOSERVICE
#define NOSOUND
#define NOTEXTMETRIC
#define NOWH
#define NOWINOFFSETS
#define NOCOMM
#define NOKANJI
#define NOHELP
#define NOPROFILER
#define NODEFERWINDOWPOS
#define NOMCX
#define NOIME
#include "windows.h"

#ifdef _MSC_VER
#define ALIGN( x ) __declspec( align( x ) )
#define MALLOC64( x ) ( ( x ) == 0 ? 0 : _aligned_malloc( ( x ), 64 ) )
#define FREE64( x ) _aligned_free( x )
#else
#define ALIGN( x ) __attribute__( ( aligned( x ) ) )
#define MALLOC64( x ) ( ( x ) == 0 ? 0 : aligned_alloc( 64, ( x ) ) )
#define FREE64( x ) free( x )
#endif
#if defined(__GNUC__) && (__GNUC__ >= 4)
#define CHECK_RESULT __attribute__ ((warn_unused_result))
#elif defined(_MSC_VER) && (_MSC_VER >= 1700)
#define CHECK_RESULT _Check_return_
#else
#define CHECK_RESULT
#endif

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "surface.h"

using namespace Tmpl8;

#include "tmpl8math.h"

#define CL_TARGET_OPENCL_VERSION 300
#include "cl/cl.h"
#include <cl/cl_gl.h>

#ifdef _MSC_VER
#include <direct.h>
#define getcwd _getcwd
#define chdir _chdir
#else
#include <unistd.h>
#endif

#define GLFW_USE_CHDIR 0
#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WGL
#include <glad.h>
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "zlib.h"

#include "opencl.h"
#include "opengl.h"

#define FATALERROR( fmt, ... ) FatalError( "Error on line %d of %s: " fmt "\n", __LINE__, __FILE__, ##__VA_ARGS__ )
#define FATALERROR_IF( condition, fmt, ... ) do { if ( ( condition ) ) FATALERROR( fmt, ##__VA_ARGS__ ); } while ( 0 )
#define FATALERROR_IN( prefix, errstr, fmt, ... ) FatalError( prefix " returned error '%s' at %s:%d" fmt "\n", errstr, __FILE__, __LINE__, ##__VA_ARGS__ );
#define FATALERROR_IN_CALL( stmt, error_parser, fmt, ... ) do { auto ret = ( stmt ); if ( ret ) FATALERROR_IN( #stmt, error_parser( ret ), fmt, ##__VA_ARGS__ ) } while ( 0 )

bool IsKeyDown( const uint key );

struct Timer
{
	Timer() { reset(); }
	float elapsed() const
	{
		chrono::high_resolution_clock::time_point t2 = chrono::high_resolution_clock::now();
		chrono::duration<double> time_span = chrono::duration_cast<chrono::duration<double>>(t2 - start);
		return (float)time_span.count();
	}
	void reset() { start = chrono::high_resolution_clock::now(); }
	chrono::high_resolution_clock::time_point start;
};

void FatalError( const char* fmt, ... );
bool FileIsNewer( const char* file1, const char* file2 );
bool FileExists( const char* f );
bool RemoveFile( const char* f );
string TextFileRead( const char* _File );
int LineCount( const string s );
void TextFileWrite( const string& text, const char* _File );

#include "common.h"


#include <iostream>
#include <bitset>
#include <array>
#include <intrin.h>

#ifdef _WIN32
#define cpuid(info, x) __cpuidex(info, x, 0)
#else
#include <cpuid.h>
void cpuid( int info[4], int InfoType ) { __cpuid_count( InfoType, 0, info[0], info[1], info[2], info[3] ); }
#endif
class CPUCaps
{
public:
	static inline bool HW_MMX = false;
	static inline bool HW_x64 = false;
	static inline bool HW_ABM = false;
	static inline bool HW_RDRAND = false;
	static inline bool HW_BMI1 = false;
	static inline bool HW_BMI2 = false;
	static inline bool HW_ADX = false;
	static inline bool HW_PREFETCHWT1 = false;
	static inline bool HW_SSE = false;
	static inline bool HW_SSE2 = false;
	static inline bool HW_SSE3 = false;
	static inline bool HW_SSSE3 = false;
	static inline bool HW_SSE41 = false;
	static inline bool HW_SSE42 = false;
	static inline bool HW_SSE4a = false;
	static inline bool HW_AES = false;
	static inline bool HW_SHA = false;
	static inline bool HW_AVX = false;
	static inline bool HW_XOP = false;
	static inline bool HW_FMA3 = false;
	static inline bool HW_FMA4 = false;
	static inline bool HW_AVX2 = false;
	static inline bool HW_AVX512F = false;
	static inline bool HW_AVX512CD = false;
	static inline bool HW_AVX512PF = false;
	static inline bool HW_AVX512ER = false;
	static inline bool HW_AVX512VL = false;
	static inline bool HW_AVX512BW = false;
	static inline bool HW_AVX512DQ = false;
	static inline bool HW_AVX512IFMA = false;
	static inline bool HW_AVX512VBMI = false;
	CPUCaps()
	{
		int info[4];
		cpuid( info, 0 );
		int nIds = info[0];
		cpuid( info, 0x80000000 );
		unsigned nExIds = info[0];
		if (nIds >= 0x00000001)
		{
			cpuid( info, 0x00000001 );
			HW_MMX = (info[3] & ((int)1 << 23)) != 0;
			HW_SSE = (info[3] & ((int)1 << 25)) != 0;
			HW_SSE2 = (info[3] & ((int)1 << 26)) != 0;
			HW_SSE3 = (info[2] & ((int)1 << 0)) != 0;
			HW_SSSE3 = (info[2] & ((int)1 << 9)) != 0;
			HW_SSE41 = (info[2] & ((int)1 << 19)) != 0;
			HW_SSE42 = (info[2] & ((int)1 << 20)) != 0;
			HW_AES = (info[2] & ((int)1 << 25)) != 0;
			HW_AVX = (info[2] & ((int)1 << 28)) != 0;
			HW_FMA3 = (info[2] & ((int)1 << 12)) != 0;
			HW_RDRAND = (info[2] & ((int)1 << 30)) != 0;
		}
		if (nIds >= 0x00000007)
		{
			cpuid( info, 0x00000007 );
			HW_AVX2 = (info[1] & ((int)1 << 5)) != 0;
			HW_BMI1 = (info[1] & ((int)1 << 3)) != 0;
			HW_BMI2 = (info[1] & ((int)1 << 8)) != 0;
			HW_ADX = (info[1] & ((int)1 << 19)) != 0;
			HW_SHA = (info[1] & ((int)1 << 29)) != 0;
			HW_PREFETCHWT1 = (info[2] & ((int)1 << 0)) != 0;
			HW_AVX512F = (info[1] & ((int)1 << 16)) != 0;
			HW_AVX512CD = (info[1] & ((int)1 << 28)) != 0;
			HW_AVX512PF = (info[1] & ((int)1 << 26)) != 0;
			HW_AVX512ER = (info[1] & ((int)1 << 27)) != 0;
			HW_AVX512VL = (info[1] & ((int)1 << 31)) != 0;
			HW_AVX512BW = (info[1] & ((int)1 << 30)) != 0;
			HW_AVX512DQ = (info[1] & ((int)1 << 17)) != 0;
			HW_AVX512IFMA = (info[1] & ((int)1 << 21)) != 0;
			HW_AVX512VBMI = (info[2] & ((int)1 << 1)) != 0;
		}
		if (nExIds >= 0x80000001)
		{
			cpuid( info, 0x80000001 );
			HW_x64 = (info[3] & ((int)1 << 29)) != 0;
			HW_ABM = (info[2] & ((int)1 << 5)) != 0;
			HW_SSE4a = (info[2] & ((int)1 << 6)) != 0;
			HW_FMA4 = (info[2] & ((int)1 << 16)) != 0;
			HW_XOP = (info[2] & ((int)1 << 11)) != 0;
		}
	}
};

inline uint RGBF32_to_RGB8( const float3& v )
{
	uint r = (uint)(255.0f * min( 1.0f, v.x ));
	uint g = (uint)(255.0f * min( 1.0f, v.y ));
	uint b = (uint)(255.0f * min( 1.0f, v.z ));
	return (r << 16) + (g << 8) + b;
}

inline float3 RGB8_to_RGBF32( const uint v )
{
	float r = ((v >> 16) & 255) * (1.0f / 255.0f);
	float g = ((v >> 8) & 255) * (1.0f / 255.0f);
	float b = (v & 255) * (1.0f / 255.0f);
	return float3( r, g, b );
}

class TheApp
{
public:
	virtual ~TheApp() {}
	virtual void Init() { /* defined empty so we can omit it from the renderer */ }
	virtual void Tick( float deltaTime ) = 0;
	virtual void UI() { uiUpdated = false; }
	virtual void Shutdown() { /* defined empty so we can omit it from the renderer */ }
	virtual void MouseUp( int ) { /* defined empty so we can omit it from the renderer */ }
	virtual void MouseDown( int ) { /* defined empty so we can omit it from the renderer */ }
	virtual void MouseMove( int, int ) { /* defined empty so we can omit it from the renderer */ }
	virtual void MouseWheel( float ) { /* defined empty so we can omit it from the renderer */ }
	virtual void KeyUp( int ) { /* defined empty so we can omit it from the renderer */ }
	virtual void KeyDown( int ) { /* defined empty so we can omit it from the renderer */ }
	Surface* screen = 0;
	bool uiUpdated;
	uint end_of_base_class = 99999;
};

class DummyApp : public TheApp
{
public:
	void Tick() {}
};

#include "ray.h"
#include "scene.h"
#include "camera.h"
#include "renderer.h"
