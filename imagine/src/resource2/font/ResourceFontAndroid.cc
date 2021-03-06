/*  This file is part of Imagine.

	Imagine is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Imagine is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Imagine.  If not, see <http://www.gnu.org/licenses/> */

#define thisModuleName "res:font:android"
#include <engine-globals.h>
#include <gfx/Gfx.hh>
#include <util/strings.h>
#include <util/jni.hh>
#include <base/android/private.hh>
#include <android/bitmap.h>

#include "ResourceFontAndroid.hh"

using namespace Base;

static JavaInstMethod<jobject> jCharBitmap, jNewSize;
static JavaInstMethod<void> jFontRenderer, jApplySize, jFreeSize, jUnlockCharBitmap;
static JavaInstMethod<jboolean> jActiveChar;
static JavaInstMethod<jint> jCurrentCharXSize, jCurrentCharYSize, jCurrentCharXOffset, jCurrentCharYOffset,
	jCurrentFaceDescender, jCurrentFaceAscender, jCurrentCharXAdvance;
static jclass jFontRendererCls = nullptr;

void setupResourceFontAndroidJni(JNIEnv *jEnv, jobject jClsLoader, const JavaInstMethod<jobject> &jLoadClass)
{
	jstring classStr = jEnv->NewStringUTF("com/imagine/FontRenderer");
	jFontRendererCls = (jclass)jEnv->NewGlobalRef(jLoadClass(jEnv, jClsLoader, classStr));
	jEnv->DeleteLocalRef(classStr);
	jFontRenderer.setup(jEnv, jFontRendererCls, "<init>", "()V");
	jCharBitmap.setup(jEnv, jFontRendererCls, "charBitmap", "()Landroid/graphics/Bitmap;");
	jUnlockCharBitmap.setup(jEnv, jFontRendererCls, "unlockCharBitmap", "(Landroid/graphics/Bitmap;)V");
	jActiveChar.setup(jEnv, jFontRendererCls, "activeChar", "(I)Z");
	jCurrentCharXSize.setup(jEnv, jFontRendererCls, "currentCharXSize", "()I");
	jCurrentCharYSize.setup(jEnv, jFontRendererCls, "currentCharYSize", "()I");
	jCurrentCharXOffset.setup(jEnv, jFontRendererCls, "currentCharXOffset", "()I");
	jCurrentCharYOffset.setup(jEnv, jFontRendererCls, "currentCharYOffset", "()I");
	jCurrentCharXAdvance.setup(jEnv, jFontRendererCls, "currentCharXAdvance", "()I");
	//jCurrentFaceDescender.setup(jEnv, jFontRendererCls, "currentFaceDescender", "()I");
	//jCurrentFaceAscender.setup(jEnv, jFontRendererCls, "currentFaceAscender", "()I");
	jNewSize.setup(jEnv, jFontRendererCls, "newSize", "(I)Landroid/graphics/Paint;");
	jApplySize.setup(jEnv, jFontRendererCls, "applySize", "(Landroid/graphics/Paint;)V");
	jFreeSize.setup(jEnv, jFontRendererCls, "freeSize", "(Landroid/graphics/Paint;)V");
}

ResourceFont *ResourceFontAndroid::loadSystem()
{
	ResourceFontAndroid *inst = new ResourceFontAndroid;
	if(!inst)
	{
		logErr("out of memory");
		return nullptr;
	}

	inst->renderer = eEnv()->NewObject(jFontRendererCls, jFontRenderer.m);
	jthrowable exc = eEnv()->ExceptionOccurred();
	if(exc)
	{
		logErr("exception");
		eEnv()->ExceptionClear();
		inst->free();
		return nullptr;
	}

	return inst;
}

void ResourceFontAndroid::free ()
{
	//eEnv()->DeleteGlobalRef(renderer);
	eEnv()->DeleteLocalRef(renderer);
	delete this;
}

static const char *androidBitmapResultToStr(int result)
{
	switch(result)
	{
		case ANDROID_BITMAP_RESULT_SUCCESS: return "Success";
		case ANDROID_BITMAP_RESULT_BAD_PARAMETER: return "Bad Parameter";
		case ANDROID_BITMAP_RESULT_JNI_EXCEPTION: return "JNI Exception";
		case ANDROID_BITMAP_RESULT_ALLOCATION_FAILED: return "Allocation Failed";
		default: return "Unknown";
	}
}

void ResourceFontAndroid::charBitmap(void *&data, int &x, int &y, int &pitch)
{
	assert(!lockedBitmap);
	lockedBitmap = jCharBitmap(eEnv(), renderer);
	AndroidBitmapInfo info;
	{
		auto res = AndroidBitmap_getInfo(eEnv(), lockedBitmap, &info);
		//logMsg("AndroidBitmap_getInfo returned %s", androidBitmapResultToStr(res));
		assert(res == ANDROID_BITMAP_RESULT_SUCCESS);
		//logMsg("size %dx%d, pitch %d", info.width, info.height, info.stride);
	}
	{
		auto res = AndroidBitmap_lockPixels(eEnv(), lockedBitmap, &data);
		//logMsg("AndroidBitmap_lockPixels returned %s", androidBitmapResultToStr(res));
		assert(res == ANDROID_BITMAP_RESULT_SUCCESS);
	}
	x = info.width;
	y = info.height;
	pitch = info.stride;
}

void ResourceFontAndroid::unlockCharBitmap(void *data)
{
	AndroidBitmap_unlockPixels(eEnv(), lockedBitmap);
	jUnlockCharBitmap(eEnv(), renderer, lockedBitmap);
	eEnv()->DeleteLocalRef(lockedBitmap);
	lockedBitmap = nullptr;
}

CallResult ResourceFontAndroid::activeChar(int idx, GlyphMetrics &metrics)
{
	//logMsg("active char: %c", idx);
	if(jActiveChar(eEnv(), renderer, idx))
	{
		metrics.xSize = jCurrentCharXSize(eEnv(), renderer);
		metrics.ySize = jCurrentCharYSize(eEnv(), renderer);
		metrics.xOffset = jCurrentCharXOffset(eEnv(), renderer);
		metrics.yOffset = jCurrentCharYOffset(eEnv(), renderer);
		metrics.xAdvance = jCurrentCharXAdvance(eEnv(), renderer);
		//logMsg("char metrics: size %dx%d offset %dx%d advance %d", metrics.xSize, metrics.ySize,
		//		metrics.xOffset, metrics.yOffset, metrics.xAdvance);
		return OK;
	}
	else
	{
		logMsg("char not available");
		return INVALID_PARAMETER;
	}
}

/*int ResourceFontAndroid::currentFaceDescender () const
{ return jCurrentFaceDescender(eEnv(), renderer); }
int ResourceFontAndroid::currentFaceAscender () const
{ return jCurrentFaceAscender(eEnv(), renderer); }*/

CallResult ResourceFontAndroid::newSize (FontSettings* settings, FontSizeRef &sizeRef)
{
	sizeRef.ptr = jNewSize(eEnv(), renderer, settings->pixelHeight);
	return OK;
}
CallResult ResourceFontAndroid::applySize (FontSizeRef &sizeRef)
{
	jApplySize(eEnv(), renderer, sizeRef.ptr);
	return OK;
}
void ResourceFontAndroid::freeSize (FontSizeRef &sizeRef)
{
	jFreeSize(eEnv(), renderer, sizeRef.ptr);
	eEnv()->DeleteLocalRef((jobject)sizeRef.ptr);
}
