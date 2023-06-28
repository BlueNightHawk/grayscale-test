//
// written by BUzer for HL: Paranoia modification
//
//		2006


#include "PlatformHeaders.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "triangleapi.h"
#include "com_model.h"
#include "studio_util.h"

#define DEAD_GRAYSCALE_TIME 5


cvar_t* v_posteffects;
cvar_t* v_grayscale;
//extern cvar_t* cv_specular_nocombiners;

PFNGLACTIVETEXTUREARBPROC glActiveTextureARB;

float GetGrayscaleFactor()
{
	float grayscale = v_grayscale->value;

	if (gHUD.m_flDeadTime)
	{
		float fact = (gEngfuncs.GetClientTime() - gHUD.m_flDeadTime) / DEAD_GRAYSCALE_TIME;
		if (fact > 1)
			fact = 1;
		if (fact > grayscale)
			grayscale = fact;
	}

	return grayscale;
}


GLuint g_screen_texture = 0;
GLuint g_weights_texture = 0; // for cards with no register combiners support

bool UseRectangleTextures()
{
	// when to use this?
	// return false;

	return true;
}


void InitScreenTexture()
{
	if (g_screen_texture != 0)
		return;

	if (!UseRectangleTextures())
	{
		// just create 256x256 texture
		unsigned char* pBlankTex = new unsigned char[256 * 256 * 3];
		memset(pBlankTex, 0, 256 * 256 * 3);
		int oldbinding;
		glGenTextures(1, &g_screen_texture);
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldbinding);
		glBindTexture(GL_TEXTURE_2D, g_screen_texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, 3, 256, 256, 0, GL_RGB, GL_UNSIGNED_BYTE, pBlankTex);
		glBindTexture(GL_TEXTURE_2D, oldbinding);
		delete[] pBlankTex;
		gEngfuncs.Con_DPrintf("Grayscale: created 256x256 2D texture\n");
		return;
	}

	unsigned char* pBlankTex = new unsigned char[ScreenWidth * ScreenHeight * 3];
	memset(pBlankTex, 255, ScreenWidth * ScreenHeight * 3);

	glEnable(GL_TEXTURE_RECTANGLE_NV);
	glGenTextures(1, &g_screen_texture);
	glBindTexture(GL_TEXTURE_RECTANGLE_NV, g_screen_texture);
	glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_RECTANGLE_NV, 0, 3, ScreenWidth, ScreenHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, pBlankTex);
	glDisable(GL_TEXTURE_RECTANGLE_NV);
	gEngfuncs.Con_DPrintf("Grayscale: created %dx%d rectangle texture\n", ScreenWidth, ScreenHeight);

	delete[] pBlankTex;
}


void MakeWeightsTexture()
{
	if (g_weights_texture != 0)
		return;

	unsigned char buf[16 * 16 * 3];
	for (int i = 0; i < (16 * 16); i++)
	{
		buf[3 * i + 0] = 168;
		buf[3 * i + 1] = 202;
		buf[3 * i + 2] = 139;
	}

	int oldbinding;
	glGenTextures(1, &g_weights_texture);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldbinding);
	glBindTexture(GL_TEXTURE_2D, g_weights_texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, 3, 16, 16, 0, GL_RGB, GL_UNSIGNED_BYTE, buf);
	glBindTexture(GL_TEXTURE_2D, oldbinding);
	gEngfuncs.Con_DPrintf("Grayscale: created weights texture\n");
}


void DrawQuad(int width, int height, int ofsX = 0, int ofsY = 0)
{
	glTexCoord2f(ofsX, ofsY);
	glVertex3f(0, 1, -1);
	glTexCoord2f(ofsX, height + ofsY);
	glVertex3f(0, 0, -1);
	glTexCoord2f(width + ofsX, height + ofsY);
	glVertex3f(1, 0, -1);
	glTexCoord2f(width + ofsX, ofsY);
	glVertex3f(1, 1, -1);
}



void ApplyPostEffects()
{
//	if (!IsGLAllowed() || !v_posteffects->value)
	//	return;

	float grayscale = GetGrayscaleFactor();
	if (grayscale <= 0 || grayscale > 1)
		return;

	InitScreenTexture();
	int texenvmode1, texenvmode2, oldbinding1, oldbinding2;

	// setup ortho projection
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(0, 1, 1, 0, 0.1, 100);

	bool blendenabled = glIsEnabled(GL_BLEND);
	glEnable(GL_BLEND);
	glColor4f(1, 1, 1, grayscale);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	//
	// Setup grayscale shader
	//
#if 0
	if (NV_combiners_supported && !cv_specular_nocombiners->value)
	{
		// use combiners
		GLfloat grayscale_weights[] = {0.320000, 0.590000, 0.090000, 0.000000};
		glCombinerParameteriNV(GL_NUM_GENERAL_COMBINERS_NV, 1);

		// RC 1 setup:
		// spare0.rgb = dot(tex0.rgb, {0.32, 0.59, 0.09})
		glCombinerParameterfvNV(GL_CONSTANT_COLOR0_NV, grayscale_weights);
		glCombinerInputNV(GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_A_NV, GL_CONSTANT_COLOR0_NV,
			GL_SIGNED_IDENTITY_NV, GL_RGB);
		glCombinerInputNV(GL_COMBINER0_NV, GL_RGB, GL_VARIABLE_B_NV, GL_TEXTURE0_ARB,
			GL_SIGNED_IDENTITY_NV, GL_RGB);
		glCombinerOutputNV(GL_COMBINER0_NV, GL_RGB,
			GL_SPARE0_NV,  // AB output
			GL_DISCARD_NV, // CD output
			GL_DISCARD_NV, // sum output
			GL_NONE, GL_NONE,
			GL_TRUE, // AB = A dot B
			GL_FALSE, GL_FALSE);

		// Final RC setup:
		//	out.rgb = spare0.rgb
		//	out.a = spare0.a
		glFinalCombinerInputNV(GL_VARIABLE_A_NV, GL_ZERO,
			GL_UNSIGNED_IDENTITY_NV, GL_RGB);
		glFinalCombinerInputNV(GL_VARIABLE_B_NV, GL_ZERO,
			GL_UNSIGNED_IDENTITY_NV, GL_RGB);
		glFinalCombinerInputNV(GL_VARIABLE_C_NV, GL_ZERO,
			GL_UNSIGNED_IDENTITY_NV, GL_RGB);
		glFinalCombinerInputNV(GL_VARIABLE_D_NV, GL_SPARE0_NV,
			GL_UNSIGNED_IDENTITY_NV, GL_RGB);
		glFinalCombinerInputNV(GL_VARIABLE_E_NV, GL_ZERO,
			GL_UNSIGNED_IDENTITY_NV, GL_RGB);
		glFinalCombinerInputNV(GL_VARIABLE_F_NV, GL_ZERO,
			GL_UNSIGNED_IDENTITY_NV, GL_RGB);
		glFinalCombinerInputNV(GL_VARIABLE_G_NV, GL_PRIMARY_COLOR_NV,
			GL_UNSIGNED_IDENTITY_NV, GL_ALPHA);

		glEnable(GL_REGISTER_COMBINERS_NV);
	}
	else
#endif
	{
		// use env_dot3
		MakeWeightsTexture();

		// 1st TU:
		// out = texture * 0.5 + 0.5
		glActiveTextureARB(GL_TEXTURE0_ARB);
		glGetTexEnviv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, &texenvmode1);

		GLfloat temp[] = {0.5, 0.5, 0.5, 0.5};
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
		glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, temp);
		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_INTERPOLATE_ARB);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PRIMARY_COLOR_ARB); // {1, 1, 1}
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_RGB_ARB, GL_CONSTANT_ARB);		// {0.5, 0.5, 0.5}
		glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 1);

		// 2nd TU:
		// make dot3
		glActiveTextureARB(GL_TEXTURE1_ARB);
		glGetTexEnviv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, &texenvmode2);
		glEnable(GL_TEXTURE_2D);
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB);
		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_DOT3_RGB_ARB);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_PREVIOUS_ARB);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_TEXTURE);
		glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 1);

		glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldbinding2);
		glBindTexture(GL_TEXTURE_2D, g_weights_texture);

		glActiveTextureARB(GL_TEXTURE0_ARB);
	}

	// copy screen to texture
	if (UseRectangleTextures())
	{
		glEnable(GL_TEXTURE_RECTANGLE_NV);
		glBindTexture(GL_TEXTURE_RECTANGLE_NV, g_screen_texture);
		//	glCopyTexImage2D(GL_TEXTURE_RECTANGLE_NV, 0, GL_RGB, 0, 0, ScreenWidth, ScreenHeight, 0);
		glCopyTexSubImage2D(GL_TEXTURE_RECTANGLE_NV, 0, 0, 0, 0, 0, ScreenWidth, ScreenHeight);

		glBegin(GL_QUADS);
		DrawQuad(ScreenWidth, ScreenHeight);
		glEnd();

		glDisable(GL_TEXTURE_RECTANGLE_NV);
	}
	else
	{
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldbinding1);
		glBindTexture(GL_TEXTURE_2D, g_screen_texture);

		int ofsy2 = ScreenHeight;
		for (int ofsy = 0; ofsy < ScreenHeight; ofsy += 256, ofsy2 -= 256)
		{
			for (int ofsx = 0; ofsx < ScreenWidth; ofsx += 256)
			{
				glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, ofsx, ofsy, 256, 256);
				glBegin(GL_QUADS);
				glTexCoord2f(0, 0);
				glVertex3f(ofsx / (float)ScreenWidth, ofsy2 / (float)ScreenHeight, -1);

				glTexCoord2f(1, 0);
				glVertex3f((ofsx + 256) / (float)ScreenWidth, ofsy2 / (float)ScreenHeight, -1);

				glTexCoord2f(1, 1);
				glVertex3f((ofsx + 256) / (float)ScreenWidth, (ofsy2 - 256) / (float)ScreenHeight, -1);

				glTexCoord2f(0, 1);
				glVertex3f(ofsx / (float)ScreenWidth, (ofsy2 - 256) / (float)ScreenHeight, -1);
				glEnd();
			}
		}

		glBindTexture(GL_TEXTURE_2D, oldbinding1);
	}
	#if 0
	if (NV_combiners_supported && !cv_specular_nocombiners->value)
	{
		glDisable(GL_REGISTER_COMBINERS_NV);
	}
	else
	#endif
	{
		glActiveTextureARB(GL_TEXTURE1_ARB);
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, texenvmode2);
		glBindTexture(GL_TEXTURE_2D, oldbinding2);
		glDisable(GL_TEXTURE_2D);
		glActiveTextureARB(GL_TEXTURE0_ARB);
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, texenvmode1);
	}

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	if (!blendenabled)
		glDisable(GL_BLEND);
}



void InitPostEffects()
{
	glActiveTextureARB = (PFNGLACTIVETEXTUREARBPROC)SDL_GL_GetProcAddress("glActiveTextureARB");

	v_posteffects = gEngfuncs.pfnRegisterVariable("gl_posteffects", "1", 0);
	v_grayscale = gEngfuncs.pfnRegisterVariable("gl_grayscale", "0", 0);
}