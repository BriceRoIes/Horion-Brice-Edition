﻿#include "DrawUtils.h"

#include "Module/ModuleManager.h"
#include <Windows.h>
#include "../Utils/Logger.h"
#include "../Utils/ClientColors.h"

struct MaterialPtr {
	char padding[0x138];
};

using tess_vertex_t = void(__fastcall*)(Tessellator* _this, float v1, float v2, float v3);
using meshHelper_renderImm_t = void(__fastcall*)(__int64, Tessellator* tesselator, MaterialPtr*);

MinecraftUIRenderContext* renderCtx;
GuiData* guiData;
__int64 screenContext2d;
__int64 game3dContext;
Tessellator* tesselator;
float* colorHolder;
std::shared_ptr<glmatrixf> refdef;
Vec2 fov;
Vec2 screenSize;
Vec3 origin;
float lerpT;
TexturePtr* texturePtr = nullptr;

static MaterialPtr* uiMaterial = nullptr;
static MaterialPtr* entityFlatStaticMaterial = nullptr;

tess_vertex_t tess_vertex;
meshHelper_renderImm_t meshHelper_renderImm;
//mce__VertexFormat__disableHalfFloats_t mce__VertexFormat__disableHalfFloats;
//Tessellator__initializeFormat_t Tessellator__initializeFormat;

bool hasInitializedSigs = false;
void initializeSigs() {
	
	tess_vertex = reinterpret_cast<tess_vertex_t>(FindSignature("40 57 48 83 EC ? 0F 29 74 24 ? 0F 29 7C 24"));
	meshHelper_renderImm = reinterpret_cast<meshHelper_renderImm_t>(FindSignature("40 53 56 57 48 81 EC ?? ?? ?? ?? 49 8B F0 48 8B DA"));
	//mce__VertexFormat__disableHalfFloats = reinterpret_cast<mce__VertexFormat__disableHalfFloats_t>(FindSignature("40 53 48 83 EC ?? 48 8B D9 C7 81 ?? ?? ?? ?? 00 00 00 00 C6 81 ?? ?? ?? ?? 00"));
	//Tessellator__initializeFormat = reinterpret_cast<Tessellator__initializeFormat_t>(FindSignature("48 89 74 24 ?? 57 48 83 EC 20 4C 8B 41 ?? 48 8B FA 4C 2B 41 ?? 48 8B F1 48 83 C1 08 ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? 49 F7 E8 48 D1 FA 48 8B C2 48 C1 E8 3F 48 03 D0 48 3B FA"));
	hasInitializedSigs = true;
}

void DrawUtils::setCtx(MinecraftUIRenderContext* ctx, GuiData* gui) {
	if (!hasInitializedSigs)
		initializeSigs();
	LARGE_INTEGER EndingTime, ElapsedMicroseconds;
	LARGE_INTEGER Frequency;
	QueryPerformanceFrequency(&Frequency);
	QueryPerformanceCounter(&EndingTime);
	ElapsedMicroseconds.QuadPart = EndingTime.QuadPart - Game.getLastUpdateTime();

	ElapsedMicroseconds.QuadPart *= 1000000;
	int ticksPerSecond = 20;
	if(Game.getClientInstance()->minecraft)
		if (Game.getClientInstance()->minecraft->timer != nullptr)
			ticksPerSecond = (int)*Game.getClientInstance()->minecraft->timer;
	if(ticksPerSecond < 1)
		ticksPerSecond = 1;
	ElapsedMicroseconds.QuadPart /= Frequency.QuadPart / ticksPerSecond;
	lerpT = (ElapsedMicroseconds.QuadPart / 1000000.f);
	if (lerpT > 1)
		lerpT = 1;
	else if (lerpT < 0)
		lerpT = 0;

	guiData = gui;
	renderCtx = ctx;
	screenContext2d = reinterpret_cast<__int64*>(renderCtx)[2];

	tesselator = *reinterpret_cast<Tessellator**>(screenContext2d + 0xB0);
	colorHolder = *reinterpret_cast<float**>(screenContext2d + 0x30);

	glmatrixf* badrefdef = Game.getClientInstance()->getRefDef();

	refdef = std::shared_ptr<glmatrixf>(badrefdef->correct());
	fov = Game.getClientInstance()->getFov();
	screenSize.x = gui->widthGame;
	screenSize.y = gui->heightGame;
	if (Game.getClientInstance()->levelRenderer != nullptr)
		origin = Game.getClientInstance()->levelRenderer->getOrigin();

	if (uiMaterial == nullptr) {
		// 2 Sigs, wanted one comes first
		uintptr_t sigOffset = FindSignature("4C 8D 05 ?? ?? ?? ?? 48 8B D3 48 8B CF 48 8B 5C 24 ?? 0F 28 7C 24 ?? 44 0F 28 44 24 ?? 48");
		int offset = *reinterpret_cast<int*>(sigOffset + 3);
		uiMaterial = reinterpret_cast<MaterialPtr*>(sigOffset + offset + 7);
	}
	if(entityFlatStaticMaterial == nullptr && Game.isInGame()) entityFlatStaticMaterial = reinterpret_cast<MaterialPtr*>(Game.getClientInstance()->itemInHandRenderer->entityLineMaterial.materialPtr);
}

void DrawUtils::setColor(float r, float g, float b, float a) {
	colorHolder[0] = r;
	colorHolder[1] = g;
	colorHolder[2] = b;
	colorHolder[3] = a;
	*reinterpret_cast<uint8_t*>(colorHolder + 4) = 1;
}

Font* DrawUtils::getFont(Fonts font) {
	static auto fontChangerModule = moduleMgr->getModule<FontChanger>();

	if (fontChangerModule->Fonts.selected == 1)
		return Game.getClientInstance()->minecraftGame->mcFont;
	else
		return Game.getClientInstance()->minecraftGame->getOldFont();

	switch (font) {
	case Fonts::SMOOTH:
		return Game.getClientInstance()->minecraftGame->getTheGoodFontThankYou();
		break;
	case Fonts::UNICOD:
		return Game.getClientInstance()->minecraftGame->getTheBetterFontYes();
		break;
	case Fonts::RUNE:
		return Game.getClientInstance()->_getRuneFont();
		break;
	case Fonts::MCFONT:
		return Game.getClientInstance()->minecraftGame->mcFont;
		break;
	default:
		return Game.getClientInstance()->_getFont();
		break;
	}
}

Tessellator* DrawUtils::get3dTessellator() {
	auto myTess = *reinterpret_cast<Tessellator**>(game3dContext + 0xB0);
	return myTess;
}

__int64 DrawUtils::getScreenContext() {
	return game3dContext == 0 ? screenContext2d : game3dContext;
}

MatrixStack* DrawUtils::getMatrixStack() {
	return reinterpret_cast<MatrixStack*>(*reinterpret_cast<__int64*>(DrawUtils::getScreenContext() + 0x18i64) + 0x30i64);
}

float DrawUtils::getTextWidth(std::string* textStr, float textSize, Fonts font) {
	TextHolder text(*textStr);

	Font* fontPtr = getFont(font);

	float ret = renderCtx->getLineLength(fontPtr, &text, textSize);

	return ret;
}

float DrawUtils::getFontHeight(float textSize, Fonts font) {
	Font* fontPtr = getFont(font);

	float ret = fontPtr->getLineHeight() * textSize;

	return ret;
}

void DrawUtils::flush() {
	renderCtx->flushText(0);
}

void DrawUtils::drawTriangle(const Vec2& p1, const Vec2& p2, const Vec2& p3) {
	
	DrawUtils::tess__begin(tesselator, 3, 3);

	tess_vertex(tesselator, p1.x, p1.y, 0);
	tess_vertex(tesselator, p2.x, p2.y, 0);
	tess_vertex(tesselator, p3.x, p3.y, 0);

	meshHelper_renderImm(screenContext2d, tesselator, uiMaterial);
}


void DrawUtils::drawQuad(const Vec2& p1, const Vec2& p2, const Vec2& p3, const Vec2& p4) {
	DrawUtils::tess__begin(tesselator, 1, 4);

	tess_vertex(tesselator, p1.x, p1.y, 0);
	tess_vertex(tesselator, p2.x, p2.y, 0);
	tess_vertex(tesselator, p3.x, p3.y, 0);
	tess_vertex(tesselator, p4.x, p4.y, 0);

	meshHelper_renderImm(screenContext2d, tesselator, uiMaterial);
}

void DrawUtils::drawLine(const Vec2& start, const Vec2& end, float lineWidth) {
	float modX = 0 - (start.y - end.y);
	float modY = start.x - end.x;

	float len = sqrtf(modX * modX + modY * modY);

	modX /= len;
	modY /= len;
	modX *= lineWidth;
	modY *= lineWidth;

	DrawUtils::tess__begin(tesselator, 3, 6);

	tess_vertex(tesselator, start.x + modX, start.y + modY, 0);
	tess_vertex(tesselator, start.x - modX, start.y - modY, 0);
	tess_vertex(tesselator, end.x - modX, end.y - modY, 0);

	tess_vertex(tesselator, start.x + modX, start.y + modY, 0);
	tess_vertex(tesselator, end.x + modX, end.y + modY, 0);
	tess_vertex(tesselator, end.x - modX, end.y - modY, 0);

	meshHelper_renderImm(screenContext2d, tesselator, uiMaterial);
}

void DrawUtils::drawText(const Vec2& pos, std::string* textStr, const MC_Color& color, float textSize, float alpha, Fonts font) {
	TextHolder text(*textStr);
	Font* fontPtr = getFont(font);
	static uintptr_t caretMeasureData = 0xFFFFFFFF;


	float posF[4];  // Vec4(startX, startY, endX, endY);
	posF[0] = pos.x;
	posF[1] = pos.x + 1000;
	posF[2] = pos.y - 1;
	posF[3] = pos.y + 1000;

	TextMeasureData textMeasure{};
	memset(&textMeasure, 0, sizeof(TextMeasureData));
	textMeasure.textSize = textSize;

	renderCtx->drawText(fontPtr, posF, &text, color.arr, alpha, 0, &textMeasure, &caretMeasureData);
}

void DrawUtils::drawBox(const Vec3& lower, const Vec3& upper, float lineWidth, bool outline) {
	
	Vec3 diff;
	diff.x = upper.x - lower.x;
	diff.y = upper.y - lower.y;
	diff.z = upper.z - lower.z;

	Vec3 vertices[8];
	vertices[0] = Vec3(lower.x, lower.y, lower.z);
	vertices[1] = Vec3(lower.x + diff.x, lower.y, lower.z);
	vertices[2] = Vec3(lower.x, lower.y + diff.y, lower.z);
	vertices[3] = Vec3(lower.x + diff.x, lower.y + diff.y, lower.z);
	vertices[4] = Vec3(lower.x, lower.y, lower.z + diff.z);
	vertices[5] = Vec3(lower.x + diff.x, lower.y, lower.z + diff.z);
	vertices[6] = Vec3(lower.x, lower.y + diff.y, lower.z + diff.z);
	vertices[7] = Vec3(lower.x + diff.x, lower.y + diff.y, lower.z + diff.z);

	// Convert to screen coord
	std::vector<std::tuple<int, Vec2>> screenCords;
	for (int i = 0; i < 8; i++) {
		Vec2 screen;
		if (refdef->OWorldToScreen(origin, vertices[i], screen, fov, screenSize)) {
			screenCords.emplace_back(outline ? (int) screenCords.size() : i, screen);
		}
	}
	if (screenCords.size() < 2)
		return;  // No lines possible

	if (!outline) {
		for (auto it = screenCords.begin(); it != screenCords.end(); it++) {
			auto from = *it;
			auto fromOrig = vertices[std::get<0>(from)];

			for (auto to : screenCords) {
				auto toOrig = vertices[std::get<0>(to)];

				bool shouldDraw = false;
				// X direction
				shouldDraw |= fromOrig.y == toOrig.y && fromOrig.z == toOrig.z && fromOrig.x < toOrig.x; 
				// Y direction
				shouldDraw |= fromOrig.x == toOrig.x && fromOrig.z == toOrig.z && fromOrig.y < toOrig.y; 
				// Z direction
				shouldDraw |= fromOrig.x == toOrig.x && fromOrig.y == toOrig.y && fromOrig.z < toOrig.z; 
				
				if (shouldDraw)
					drawLine(std::get<1>(from), std::get<1>(to), lineWidth);
			}
		}

		return;
	}
	// Find start vertex
	auto it = screenCords.begin();
	std::tuple<int, Vec2> start = *it;
	it++;
	for (; it != screenCords.end(); it++) {
		auto cur = *it;
		if (std::get<1>(cur).x < std::get<1>(start).x) {
			start = cur;
		}
	}

	// Follow outer line
	std::vector<int> indices;

	auto current = start;
	indices.push_back(std::get<0>(current));
	Vec2 lastDir(0, -1);
	do {
		float smallestAngle = PI * 2;
		Vec2 smallestDir;
		std::tuple<int, Vec2> smallestE;
		auto lastDirAtan2 = atan2(lastDir.y, lastDir.x);
		for (auto cur : screenCords) {
			if (std::get<0>(current) == std::get<0>(cur))
				continue;

			// angle between vecs
			Vec2 dir = Vec2(std::get<1>(cur)).sub(std::get<1>(current));
			float angle = atan2(dir.y, dir.x) - lastDirAtan2;
			if (angle > PI) {
				angle -= 2 * PI;
			} else if (angle <= -PI) {
				angle += 2 * PI;
			}
			if (angle >= 0 && angle < smallestAngle) {
				smallestAngle = angle;
				smallestDir = dir;
				smallestE = cur;
			}
		}
		indices.push_back(std::get<0>(smallestE));
		lastDir = smallestDir;
		current = smallestE;
	} while (std::get<0>(current) != std::get<0>(start) && indices.size() < 8);

	// draw
	
	Vec2 lastVertex;
	bool hasLastVertex = false;
	for (auto& indice : indices) {
		Vec2 curVertex = std::get<1>(screenCords[indice]);
		if (!hasLastVertex) {
			hasLastVertex = true;
			lastVertex = curVertex;
			continue;
		}
		
		drawLine(lastVertex, curVertex, lineWidth);
		lastVertex = curVertex;
	}
}

void DrawUtils::drawImage(std::string filePath, Vec2& imagePos, Vec2& ImageDimension, Vec2& idk) {
	if (texturePtr == nullptr) {
		texturePtr = new TexturePtr();
		FilePath file(filePath);
		renderCtx->getTexture(texturePtr, file);
	}

	__int64 yot = 0;
	static unsigned __int64 hashedString = 0xA99285D21E94FC80;
	static uintptr_t flushImageAddr = FindSignature("48 8B C4 55 56 57 41 54 41 55 41 56 41 57 ?? ?? ?? ?? ?? ?? ?? 48 ?? ?? ?? ?? ?? ?? 48 ?? ?? ?? ?? ?? ?? ?? 48 89 58 ?? 0F 29 70 ?? 0F 29 78 ?? 44 0F 29 40 ?? 48 8B 05 ?? ?? ?? ?? 48 33 C4 ?? ?? ?? ?? ?? ?? ?? 4D 8B E1 44 0F 28 C2 4C 8B F2 4C 8B F9");

	if (texturePtr != nullptr) {
		renderCtx->drawImage(texturePtr, imagePos, ImageDimension, yot, idk);
		MC_Color col(1.f, 1.f, 1.f);
		renderCtx->flushImages(col, flushImageAddr, (__int64)&hashedString);
	}
}

void DrawUtils::drawNameTags(Entity* ent, float textSize, bool drawHealth, bool useUnicodeFont) {
	Vec2 textPos;
	Vec4 rectPos;
	std::string text = ent->getNameTag()->getText();
	text = Utils::sanitize(text);

	float textWidth = getTextWidth(&text, textSize);
	float textHeight = DrawUtils::getFont(Fonts::SMOOTH)->getLineHeight() * textSize;

	if (refdef->OWorldToScreen(origin, ent->eyePos0.add(0, 0.5f, 0), textPos, fov, screenSize)) {
		textPos.y -= textHeight;
		textPos.x -= textWidth / 2.f;
		rectPos.x = textPos.x - 1.f * textSize;
		rectPos.y = textPos.y - 1.f * textSize;
		rectPos.z = textPos.x + textWidth + 1.f * textSize;
		rectPos.w = textPos.y + textHeight + 2.f * textSize;
		Vec4 subRectPos = rectPos;
		subRectPos.y = subRectPos.w - 1.f * textSize;
		static auto nametagsMod = moduleMgr->getModule<NameTags>();
		fillRectangle(rectPos, ClientColors::nametagsBackgroundColor, nametagsMod->opacity);
		if (nametagsMod->underline) {
			fillRectangle(subRectPos, ClientColors::nametagsUnderlineColor, 0.9f);
		}
		drawText(textPos, &text, MC_Color(255, 255, 255), textSize);

		static auto nameTagsMod = moduleMgr->getModule<NameTags>();

		if (ent->getEntityTypeId() == 63 && nameTagsMod->displayArmor) {  // is player, show armor
			auto* player = reinterpret_cast<Player*>(ent);
			float scale = textSize * 0.6f;
			float spacing = scale + 15.f;
			float x = rectPos.x + 1.f * textSize;
			float y = rectPos.y - 20.f * scale;
			// armor
			for (int i = 0; i < 4; i++) {
				ItemStack* stack = player->getArmor(i);
				if (stack->item != nullptr) {
					DrawUtils::drawItem(stack, Vec2(x, y), 1.f, scale, stack->isEnchanted());
					x += scale * spacing;
				}
			}
			// item
			{
				ItemStack* stack = player->getSelectedItem();
				if (stack->item != nullptr) {
					DrawUtils::drawItem(stack, Vec2(rectPos.z - 1.f - 15.f * scale, y), 1.f, scale, stack->isEnchanted());
				}
			}
		}
	}
}

void DrawUtils::drawEntityBox(Entity* ent, float lineWidth) {
	Vec3 end = ent->eyePos0;
	AABB render(end, ent->width, ent->height, end.y - ent->aabb.lower.y);
	render.upper.y += 0.1f;

	drawBox(render.lower, render.upper, lineWidth, true);
}

void DrawUtils::draw2D(Entity* ent, float lineWidth) {
	Vec3 base = ent->eyePos0;
	float ofs = (Game.getLocalPlayer()->yaw + 90.f) * (PI / 180);

	Vec3 corners[4];
	Vec2 corners2d[4];

	corners[0] = Vec3(base.x - ent->width / 1.5f * -sin(ofs), ent->aabb.upper.y + (float)0.1, base.z - ent->width / 1.5f * cos(ofs));
	corners[1] = Vec3(base.x + ent->width / 1.5f * -sin(ofs), ent->aabb.upper.y + (float)0.1, base.z + ent->width / 1.5f * cos(ofs));
	corners[2] = Vec3(base.x - ent->width / 1.5f * -sin(ofs), ent->aabb.lower.y, base.z - ent->width / 1.5f * cos(ofs));
	corners[3] = Vec3(base.x + ent->width / 1.5f * -sin(ofs), ent->aabb.lower.y, base.z + ent->width / 1.5f * cos(ofs));

	if (refdef->OWorldToScreen(origin, corners[0], corners2d[0], fov, screenSize) &&
		refdef->OWorldToScreen(origin, corners[1], corners2d[1], fov, screenSize) &&
		refdef->OWorldToScreen(origin, corners[2], corners2d[2], fov, screenSize) &&
		refdef->OWorldToScreen(origin, corners[3], corners2d[3], fov, screenSize)) {
		//float length = (corners2d[1].x - corners2d[0].x) / 4.f;

		/*drawLine(corners2d[0], Vec2(corners2d[0].x + length, corners2d[0].y), lineWidth);
		drawLine(corners2d[0], Vec2(corners2d[0].x, corners2d[0].y + length), lineWidth);

		drawLine(Vec2(corners2d[1].x - length, corners2d[1].y), corners2d[1], lineWidth);
		drawLine(corners2d[1], Vec2(corners2d[1].x, corners2d[1].y + length), lineWidth);

		drawLine(Vec2(corners2d[2].x, corners2d[2].y - length), corners2d[2], lineWidth);
		drawLine(corners2d[2], Vec2(corners2d[2].x + length, corners2d[2].y), lineWidth);

		drawLine(Vec2(corners2d[3].x, corners2d[3].y - length), corners2d[3], lineWidth);
		drawLine(Vec2(corners2d[3].x - length, corners2d[3].y), corners2d[3], lineWidth);*/

		drawLine(corners2d[0], corners2d[1], lineWidth);
		drawLine(corners2d[0], corners2d[2], lineWidth);
		drawLine(corners2d[3], corners2d[1], lineWidth);
		drawLine(corners2d[3], corners2d[2], lineWidth);
	}
}

void DrawUtils::drawItem(ItemStack* item, const Vec2& itemPos, float opacity, float scale, bool isEnchanted) {
	__int64 scnCtx = reinterpret_cast<__int64*>(renderCtx)[2];
	auto* screenCtx = reinterpret_cast<ScreenContext*>(scnCtx);
	BaseActorRenderContext baseActorRenderCtx(screenCtx, Game.getClientInstance(), Game.getClientInstance()->minecraftGame);
	ItemRenderer* renderer = baseActorRenderCtx.renderer;
	renderer->renderGuiItemNew(&baseActorRenderCtx, item, 0, itemPos.x, itemPos.y, opacity, scale, isEnchanted);
}

Vec2 DrawUtils::worldToScreen(const Vec3& world) {
	Vec2 ret{-1, -1};
	refdef->OWorldToScreen(origin, world, ret, fov, screenSize);
	return ret;
}
void DrawUtils::drawLine3d(const Vec3& start, const Vec3& end) {
	if(game3dContext == 0 || entityFlatStaticMaterial == 0)
		return;

	auto myTess = DrawUtils::get3dTessellator();

	DrawUtils::tess__begin(myTess, 4, 2);

	auto start1 = start.sub(origin);
	auto end1 = end.sub(origin);

	tess_vertex(myTess, start1.x, start1.y, start1.z);
	tess_vertex(myTess, end1.x, end1.y, end1.z);

	meshHelper_renderImm(game3dContext, myTess, entityFlatStaticMaterial);
}
void DrawUtils::drawBox3d(const Vec3& lower, const Vec3& upper) {
	if (game3dContext == 0 || entityFlatStaticMaterial == 0)
		return;

	auto myTess = DrawUtils::get3dTessellator();

	DrawUtils::tess__begin(myTess, 4, 12);

	Vec3 diff;
	diff.x = upper.x - lower.x;
	diff.y = upper.y - lower.y;
	diff.z = upper.z - lower.z;

	auto newLower = lower.sub(origin);

	Vec3 vertices[8];
	vertices[0] = Vec3(newLower.x, newLower.y, newLower.z);
	vertices[1] = Vec3(newLower.x + diff.x, newLower.y, newLower.z);
	vertices[2] = Vec3(newLower.x, newLower.y, newLower.z + diff.z);
	vertices[3] = Vec3(newLower.x + diff.x, newLower.y, newLower.z + diff.z);

	vertices[4] = Vec3(newLower.x, newLower.y + diff.y, newLower.z);
	vertices[5] = Vec3(newLower.x + diff.x, newLower.y + diff.y, newLower.z);
	vertices[6] = Vec3(newLower.x, newLower.y + diff.y, newLower.z + diff.z);
	vertices[7] = Vec3(newLower.x + diff.x, newLower.y + diff.y, newLower.z + diff.z);

	#define line(m, n) tess_vertex(myTess, m.x, m.y, m.z); \
		tess_vertex(myTess, n.x, n.y, n.z);
	
	#define li(m, n) line(vertices[m], vertices[n]);

	li(0, 1);
	li(1, 3);
	li(3, 2);
	li(2, 0);

	li(4, 5);
	li(5, 7);
	li(7, 6);
	li(6, 4);

	li(0, 4);
	li(1, 5);
	li(2, 6);
	li(3, 7);

	#undef li
	#undef line
	
	meshHelper_renderImm(game3dContext, myTess, entityFlatStaticMaterial);
}
void DrawUtils::fillRectangle(const Vec4& pos, const MC_Color& col, float alpha) {
	DrawUtils::setColor(col.r, col.g, col.b, alpha);
	DrawUtils::drawQuad({pos.x, pos.w}, {pos.z, pos.w}, {pos.z, pos.y}, {pos.x, pos.y});
}
inline void DrawUtils::tess__begin(Tessellator* tess, int vertexFormat, int numVerticesReserved) {
	__int64 tesselator = reinterpret_cast<__int64>(tess);

	using tess_begin_t = void(__fastcall*)(Tessellator*, int, int);
	static tess_begin_t tess_begin = reinterpret_cast<tess_begin_t>(FindSignature("48 89 5C 24 ?? 55 48 83 EC ?? 80 B9 ?? ?? ?? ?? 00 45"));
	tess_begin(tess, vertexFormat, numVerticesReserved);
}
void DrawUtils::setGameRenderContext(__int64 ctx) {
	game3dContext = ctx;
	if (Game.getClientInstance()->levelRenderer != nullptr)
		origin = Game.getClientInstance()->levelRenderer->getOrigin();

	if(ctx){
		LARGE_INTEGER EndingTime, ElapsedMicroseconds;
		LARGE_INTEGER Frequency;
		QueryPerformanceFrequency(&Frequency);
		QueryPerformanceCounter(&EndingTime);
		ElapsedMicroseconds.QuadPart = EndingTime.QuadPart - Game.getLastUpdateTime();

		ElapsedMicroseconds.QuadPart *= 1000000;
		int ticksPerSecond = 20;
		if(Game.getClientInstance()->minecraft)
			ticksPerSecond = (int)*Game.getClientInstance()->minecraft->timer;
		if(ticksPerSecond < 1)
			ticksPerSecond = 1;
		ElapsedMicroseconds.QuadPart /= Frequency.QuadPart / ticksPerSecond;
		lerpT = (ElapsedMicroseconds.QuadPart / 1000000.f);
		if (lerpT > 1)
			lerpT = 1;
		else if (lerpT < 0)
			lerpT = 0;
	}
}
float DrawUtils::getLerpTime() {
	return lerpT;
}
Vec3 DrawUtils::getOrigin() {
	return origin;
}
void DrawUtils::drawLinestrip3d(const std::vector<Vec3>& points) {
	if(game3dContext == 0 || entityFlatStaticMaterial == 0)
		return;

	auto myTess = DrawUtils::get3dTessellator();

	DrawUtils::tess__begin(myTess, 5, (int)points.size());

	/*
	 * 1: quads
	 * 2: triangle list
	 * 3: trianglestrip (6)
	 * 4: line list
	 * 5: line strip (7)
	 */

	for(const auto& p : points){
		auto pD = p.sub(origin);
		tess_vertex(myTess, pD.x, pD.y, pD.z);
	}
	

	meshHelper_renderImm(game3dContext, myTess, entityFlatStaticMaterial);
}

MC_Color MC_Color::lerp(const MC_Color& o, float t) const {
	return MC_Color(Utils::lerp(r, o.r, t), Utils::lerp(g, o.g, t), Utils::lerp(b, o.b, t), Utils::lerp(a, o.a, t));
}
