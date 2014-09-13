#include "Windows/Window.h"
#include "Windows/Application.h"
#include "Core/Renderer.h"
#include "Graphics/Camera.h"
#include "Utils/Mesh.h"

#include <gl/GL.h>

using namespace EDX;
using namespace EDX::RasterRenderer;

const int giWindowWidth = 1280;
const int giWindowHeight = 720;

// Global variables
RefPtr<Renderer>		gRenderer;
Camera			gCamera;
Mesh			gMesh;

void OnInit(Object* pSender, EventArgs args)
{
	glClearColor(0.4f, 0.5f, 0.65f, 1.0f);

	gRenderer = new Renderer;
	gRenderer->Initialize(giWindowWidth, giWindowHeight);
	gCamera.Init(-5.0f * Vector3::UNIT_Z, Vector3::ZERO, Vector3::UNIT_Y, giWindowWidth, giWindowHeight, 65);

	//gMesh.LoadPlane(Vector3::ZERO, Vector3(1, 1, 1), Vector3(-90.0f, 0.0f, 0.0f), 1.2f);
	//gMesh.LoadSphere(Vector3::ZERO, Vector3::UNIT_SCALE, Vector3::ZERO, 0.8f);
	//gMesh.LoadMesh(Vector3(0, -10, 35), Vector3::UNIT_SCALE, Vector3(0, 180, 0), "../../Media/bunny.obj");
	//gMesh.LoadMesh(Vector3(0, -10, 35), 0.003f * Vector3::UNIT_SCALE, Vector3(0, 180, 0), "../../Media/venusm.obj");
	//gMesh.LoadMesh(Vector3(0, 0, 35), Vector3::UNIT_SCALE, Vector3(180, 180, 0), "../../Media/budha.obj");
	//gMesh.LoadMesh(Vector3(0, 0, 35), Vector3::UNIT_SCALE, Vector3(0, 0, 0), "../../Media/teapot.obj");
	//gMesh.LoadMesh(Vector3(0, 0, 0), Vector3::UNIT_SCALE, Vector3(0, 180, 0), "../../Media/cornell-box/cornellbox.obj");
	//gMesh.LoadMesh(Vector3(0, 0, 0), 5.0f * Vector3::UNIT_SCALE, Vector3(0, -90, 0), "../../Media/dragon.obj");
	gMesh.LoadMesh(Vector3(0, 0, 0), Vector3::UNIT_SCALE, Vector3(0, 0, 0), "../../Media/san-miguel/san-miguel.obj");
	//gMesh.LoadMesh(Vector3(0, 0, 0), 0.01f * Vector3::UNIT_SCALE, Vector3(0, 0, 0), "../../Media/sponza/sponza.obj");
	//gMesh.LoadMesh(Vector3(0, 0, 0), 0.01f * Vector3::UNIT_SCALE, Vector3(0, 0, 0), "../../Media/crytek-sponza/sponza.obj");
}

void OnRender(Object* pSender, EventArgs args)
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	gRenderer->SetRenderState(gCamera.GetViewMatrix(), gCamera.GetProjMatrix(), gCamera.GetRasterMatrix());
	gRenderer->RenderMesh(gMesh);

	glRasterPos3f(0.0f, 0.0f, 0.0f);
	glDrawPixels(giWindowWidth, giWindowHeight, GL_RGBA, GL_FLOAT, gRenderer->GetBackBuffer());

	// 	Matrix mView = gCamera.GetViewMatrix();
	// 	glLoadTransposeMatrixf((float*)&mView);
	// 
	// 	glBindBuffer(GL_ARRAY_BUFFER, giVboId1);
	// 	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, giVboId2);
	// 
	// 	glEnableClientState(GL_VERTEX_ARRAY);
	// 	glVertexPointer(3, GL_FLOAT, sizeof(MeshVertex), 0);
	// 	glEnableClientState(GL_COLOR_ARRAY);
	// 	glColorPointer(3, GL_FLOAT, sizeof(MeshVertex), reinterpret_cast<void*>(offsetof(MeshVertex, nNormal)));
	// 	
	// 	glDrawElements(GL_TRIANGLES, 3 * gMesh.GetTriangleCount(), GL_UNSIGNED_INT, 0);
	// 
	// 	glDisableClientState(GL_VERTEX_ARRAY);
	// 
	// 	glBindBuffer(GL_ARRAY_BUFFER, 0);
	// 	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void OnResize(Object* pSender, ResizeEventArgs args)
{
	glViewport(0, 0, args.Width, args.Height);

	// Set opengl params
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, args.Width, 0, args.Height, -1, 1);

	// 	Matrix mProj = gCamera.GetProjMatrix();
	// 	glLoadTransposeMatrixf((float*)&mProj);

	glMatrixMode(GL_MODELVIEW);

	gCamera.Resize(args.Width, args.Height);
}

void OnRelease(Object* pSender, EventArgs args)
{
	gRenderer.Dereference();
	gMesh.~Mesh();
}

void OnMouseEvent(Object* pSender, MouseEventArgs args)
{
	gCamera.HandleMouseMsg(args);
}

void OnKeyboardEvent(Object* pSender, KeyboardEventArgs args)
{
	gCamera.HandleKeyboardMsg(args);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR cmdArgs, int cmdShow)
{
	Application::Init(hInst);

	Window* mainWindow = new GLWindow;
	mainWindow->SetMainLoop(NotifyEvent(OnRender));
	mainWindow->SetInit(NotifyEvent(OnInit));
	mainWindow->SetResize(ResizeEvent(OnResize));
	mainWindow->SetRelease(NotifyEvent(OnRelease));
	mainWindow->SetMouseHandler(MouseEvent(OnMouseEvent));
	mainWindow->SetkeyboardHandler(KeyboardEvent(OnKeyboardEvent));

	mainWindow->Create(L"EDXRaster", giWindowWidth, giWindowHeight);

	Application::Run(mainWindow);

	return 0;
}