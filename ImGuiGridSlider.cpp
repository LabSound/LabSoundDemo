
// Source https://gist.github.com/soufianekhiat/4d937b15bf7290abed9608569d229201
// Code by Soufiane Khiat

#include "imgui-app/imgui.h"
#include <algorithm>
#include <string>
#include <cmath>

/* from ImGui internal */
namespace ImGui {
    IMGUI_API void          RenderFrame(ImVec2 p_min, ImVec2 p_max, ImU32 fill_col, bool border = true, float rounding = 0.0f);

    template<typename T> static inline T ImMin(T lhs, T rhs) { return lhs < rhs ? lhs : rhs; }
    template<typename T> static inline T ImMax(T lhs, T rhs) { return lhs >= rhs ? lhs : rhs; }
    template<typename T> static inline T ImClamp(T v, T mn, T mx) { return (v < mn) ? mn : (v > mx) ? mx : v; }
    static inline ImVec2 ImMin(const ImVec2& lhs, const ImVec2& rhs) { return ImVec2(lhs.x < rhs.x ? lhs.x : rhs.x, lhs.y < rhs.y ? lhs.y : rhs.y); }
    static inline ImVec2 ImMax(const ImVec2& lhs, const ImVec2& rhs) { return ImVec2(lhs.x >= rhs.x ? lhs.x : rhs.x, lhs.y >= rhs.y ? lhs.y : rhs.y); }
    static inline ImVec2 ImClamp(const ImVec2& v, const ImVec2& mn, ImVec2 mx) { return ImVec2((v.x < mn.x) ? mn.x : (v.x > mx.x) ? mx.x : v.x, (v.y < mn.y) ? mn.y : (v.y > mx.y) ? mx.y : v.y); }
#define IM_FLOOR(_VAL)                  ((float)(int)(_VAL))                                    // ImFloor() is not inlined in MSVC debug builds


    // Helper: ImRect (2D axis aligned bounding-box)
    // NB: we can't rely on ImVec2 math operators being available here!
    struct IMGUI_API ImRect
    {
        ImVec2      Min;    // Upper-left
        ImVec2      Max;    // Lower-right

        ImRect() : Min(0.0f, 0.0f), Max(0.0f, 0.0f) {}
        ImRect(const ImVec2& min, const ImVec2& max) : Min(min), Max(max) {}
        ImRect(const ImVec4& v) : Min(v.x, v.y), Max(v.z, v.w) {}
        ImRect(float x1, float y1, float x2, float y2) : Min(x1, y1), Max(x2, y2) {}

        ImVec2      GetCenter() const { return ImVec2((Min.x + Max.x) * 0.5f, (Min.y + Max.y) * 0.5f); }
        ImVec2      GetSize() const { return ImVec2(Max.x - Min.x, Max.y - Min.y); }
        float       GetWidth() const { return Max.x - Min.x; }
        float       GetHeight() const { return Max.y - Min.y; }
        ImVec2      GetTL() const { return Min; }                   // Top-left
        ImVec2      GetTR() const { return ImVec2(Max.x, Min.y); }  // Top-right
        ImVec2      GetBL() const { return ImVec2(Min.x, Max.y); }  // Bottom-left
        ImVec2      GetBR() const { return Max; }                   // Bottom-right
        bool        Contains(const ImVec2& p) const { return p.x >= Min.x && p.y >= Min.y && p.x < Max.x&& p.y < Max.y; }
        bool        Contains(const ImRect& r) const { return r.Min.x >= Min.x && r.Min.y >= Min.y && r.Max.x <= Max.x && r.Max.y <= Max.y; }
        bool        Overlaps(const ImRect& r) const { return r.Min.y <  Max.y&& r.Max.y >  Min.y && r.Min.x <  Max.x&& r.Max.x >  Min.x; }
        void        Add(const ImVec2& p) { if (Min.x > p.x)     Min.x = p.x;     if (Min.y > p.y)     Min.y = p.y;     if (Max.x < p.x)     Max.x = p.x;     if (Max.y < p.y)     Max.y = p.y; }
        void        Add(const ImRect& r) { if (Min.x > r.Min.x) Min.x = r.Min.x; if (Min.y > r.Min.y) Min.y = r.Min.y; if (Max.x < r.Max.x) Max.x = r.Max.x; if (Max.y < r.Max.y) Max.y = r.Max.y; }
        void        Expand(const float amount) { Min.x -= amount;   Min.y -= amount;   Max.x += amount;   Max.y += amount; }
        void        Expand(const ImVec2& amount) { Min.x -= amount.x; Min.y -= amount.y; Max.x += amount.x; Max.y += amount.y; }
        void        Translate(const ImVec2& d) { Min.x += d.x; Min.y += d.y; Max.x += d.x; Max.y += d.y; }
        void        TranslateX(float dx) { Min.x += dx; Max.x += dx; }
        void        TranslateY(float dy) { Min.y += dy; Max.y += dy; }
        void        ClipWith(const ImRect& r) { Min = ImMax(Min, r.Min); Max = ImMin(Max, r.Max); }                   // Simple version, may lead to an inverted rectangle, which is fine for Contains/Overlaps test but not for display.
        void        ClipWithFull(const ImRect& r) { Min = ImClamp(Min, r.Min, r.Max); Max = ImClamp(Max, r.Min, r.Max); } // Full version, ensure both points are fully clipped.
        void        Floor() { Min.x = IM_FLOOR(Min.x); Min.y = IM_FLOOR(Min.y); Max.x = IM_FLOOR(Max.x); Max.y = IM_FLOOR(Max.y); }
        bool        IsInverted() const { return Min.x > Max.x || Min.y > Max.y; }
        ImVec4      ToVec4() const { return ImVec4(Min.x, Min.y, Max.x, Max.y); }
    };

    static inline ImVec2 operator*(const ImVec2& lhs, const float rhs) { return ImVec2(lhs.x * rhs, lhs.y * rhs); }
    static inline ImVec2 operator/(const ImVec2& lhs, const float rhs) { return ImVec2(lhs.x / rhs, lhs.y / rhs); }
    static inline ImVec2 operator+(const ImVec2& lhs, const ImVec2& rhs) { return ImVec2(lhs.x + rhs.x, lhs.y + rhs.y); }
    static inline ImVec2 operator-(const ImVec2& lhs, const ImVec2& rhs) { return ImVec2(lhs.x - rhs.x, lhs.y - rhs.y); }
    static inline ImVec2 operator*(const ImVec2& lhs, const ImVec2& rhs) { return ImVec2(lhs.x * rhs.x, lhs.y * rhs.y); }
    static inline ImVec2 operator/(const ImVec2& lhs, const ImVec2& rhs) { return ImVec2(lhs.x / rhs.x, lhs.y / rhs.y); }
    static inline ImVec2& operator*=(ImVec2& lhs, const float rhs) { lhs.x *= rhs; lhs.y *= rhs; return lhs; }
    static inline ImVec2& operator/=(ImVec2& lhs, const float rhs) { lhs.x /= rhs; lhs.y /= rhs; return lhs; }
    static inline ImVec2& operator+=(ImVec2& lhs, const ImVec2& rhs) { lhs.x += rhs.x; lhs.y += rhs.y; return lhs; }
    static inline ImVec2& operator-=(ImVec2& lhs, const ImVec2& rhs) { lhs.x -= rhs.x; lhs.y -= rhs.y; return lhs; }
    static inline ImVec2& operator*=(ImVec2& lhs, const ImVec2& rhs) { lhs.x *= rhs.x; lhs.y *= rhs.y; return lhs; }
    static inline ImVec2& operator/=(ImVec2& lhs, const ImVec2& rhs) { lhs.x /= rhs.x; lhs.y /= rhs.y; return lhs; }
    static inline ImVec4 operator+(const ImVec4& lhs, const ImVec4& rhs) { return ImVec4(lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z, lhs.w + rhs.w); }
    static inline ImVec4 operator-(const ImVec4& lhs, const ImVec4& rhs) { return ImVec4(lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z, lhs.w - rhs.w); }
    static inline ImVec4 operator*(const ImVec4& lhs, const ImVec4& rhs) { return ImVec4(lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z, lhs.w * rhs.w); }
}
/* from ImGui internal */

using namespace ImGui;

bool SliderScalar2D(char const* pLabel, float* fValueX, float* fValueY, const float fMinX, const float fMaxX, const float fMinY, const float fMaxY, float const fZoom /*= 1.0f*/)
{
    assert(fMinX < fMaxX);
    assert(fMinY < fMaxY);

    ImGuiID const iID = ImGui::GetID(pLabel);

    ImVec2 const vSizeSubstract = ImGui::CalcTextSize(std::to_string(1.0f).c_str()) * 1.1f;

    float const vSizeFull = (ImGui::GetWindowContentRegionWidth() - vSizeSubstract.x) * fZoom;
    ImVec2 const vSize(vSizeFull, vSizeFull);

    float const fHeightOffset = ImGui::GetTextLineHeight();
    ImVec2 const vHeightOffset(0.0f, fHeightOffset);

    ImVec2 vPos = ImGui::GetCursorScreenPos();
    ImRect oRect(vPos + vHeightOffset, vPos + vSize + vHeightOffset);

    ImGui::TextUnformatted(pLabel);

    ImGui::PushID(iID);

    ImU32 const uFrameCol = ImGui::GetColorU32(ImGuiCol_FrameBg);

    ImVec2 const vOriginPos = ImGui::GetCursorScreenPos();
    ImGui::RenderFrame(oRect.Min, oRect.Max, uFrameCol, false, 0.0f);

    float const fDeltaX = fMaxX - fMinX;
    float const fDeltaY = fMaxY - fMinY;

    bool bModified = false;
    ImVec2 const vSecurity(15.0f, 15.0f);
    if (ImGui::IsMouseHoveringRect(oRect.Min - vSecurity, oRect.Max + vSecurity) && ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        ImVec2 const vCursorPos = ImGui::GetMousePos() - oRect.Min;

        *fValueX = vCursorPos.x / (oRect.Max.x - oRect.Min.x) * fDeltaX + fMinX;
        *fValueY = fDeltaY - vCursorPos.y / (oRect.Max.y - oRect.Min.y) * fDeltaY + fMinY;

        bModified = true;
    }

    *fValueX = std::min(std::max(*fValueX, fMinX), fMaxX);
    *fValueY = std::min(std::max(*fValueY, fMinY), fMaxY);

    float const fScaleX = (*fValueX - fMinX) / fDeltaX;
    float const fScaleY = 1.0f - (*fValueY - fMinY) / fDeltaY;

    constexpr float fCursorOff = 10.0f;
    float const fXLimit = fCursorOff / oRect.GetWidth();
    float const fYLimit = fCursorOff / oRect.GetHeight();

    ImVec2 const vCursorPos((oRect.Max.x - oRect.Min.x) * fScaleX + oRect.Min.x, (oRect.Max.y - oRect.Min.y) * fScaleY + oRect.Min.y);

    ImDrawList* pDrawList = ImGui::GetWindowDrawList();

    ImVec4 const vBlue(70.0f / 255.0f, 102.0f / 255.0f, 230.0f / 255.0f, 1.0f); // TODO: choose from style
    ImVec4 const vOrange(255.0f / 255.0f, 128.0f / 255.0f, 64.0f / 255.0f, 1.0f); // TODO: choose from style

    ImS32 const uBlue = ImGui::GetColorU32(vBlue);
    ImS32 const uOrange = ImGui::GetColorU32(vOrange);

    constexpr float fBorderThickness = 2.0f;
    constexpr float fLineThickness = 3.0f;
    constexpr float fHandleRadius = 7.0f;
    constexpr float fHandleOffsetCoef = 2.0f;

    // Cursor
    pDrawList->AddCircleFilled(vCursorPos, 5.0f, uBlue, 16);

    // Vertical Line
    if (fScaleY > 2.0f * fYLimit)
        pDrawList->AddLine(ImVec2(vCursorPos.x, oRect.Min.y + fCursorOff), ImVec2(vCursorPos.x, vCursorPos.y - fCursorOff), uOrange, fLineThickness);
    if (fScaleY < 1.0f - 2.0f * fYLimit)
        pDrawList->AddLine(ImVec2(vCursorPos.x, oRect.Max.y - fCursorOff), ImVec2(vCursorPos.x, vCursorPos.y + fCursorOff), uOrange, fLineThickness);

    // Horizontal Line
    if (fScaleX > 2.0f * fXLimit)
        pDrawList->AddLine(ImVec2(oRect.Min.x + fCursorOff, vCursorPos.y), ImVec2(vCursorPos.x - fCursorOff, vCursorPos.y), uOrange, fLineThickness);
    if (fScaleX < 1.0f - 2.0f * fYLimit)
        pDrawList->AddLine(ImVec2(oRect.Max.x - fCursorOff, vCursorPos.y), ImVec2(vCursorPos.x + fCursorOff, vCursorPos.y), uOrange, fLineThickness);

    char pBufferX[16];
    char pBufferY[16];
    snprintf(pBufferX, IM_ARRAYSIZE(pBufferX), "%.5f", *(float const*)fValueX);
    snprintf(pBufferY, IM_ARRAYSIZE(pBufferY), "%.5f", *(float const*)fValueY);

    ImU32 const uTextCol = ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[ImGuiCol_Text]);

    ImGui::SetWindowFontScale(0.75f);

    ImVec2 const vXSize = ImGui::CalcTextSize(pBufferX);
    ImVec2 const vYSize = ImGui::CalcTextSize(pBufferY);

    ImVec2 const vHandlePosX = ImVec2(vCursorPos.x, oRect.Max.y + vYSize.x * 0.5f);
    ImVec2 const vHandlePosY = ImVec2(oRect.Max.x + fHandleOffsetCoef * fCursorOff + vYSize.x, vCursorPos.y);

    if (ImGui::IsMouseHoveringRect(vHandlePosX - ImVec2(fHandleRadius, fHandleRadius) - vSecurity, vHandlePosX + ImVec2(fHandleRadius, fHandleRadius) + vSecurity) &&
        ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        ImVec2 const vCursorPos = ImGui::GetMousePos() - oRect.Min;

        *fValueX = vCursorPos.x / (oRect.Max.x - oRect.Min.x) * fDeltaX + fMinX;

        bModified = true;
    }
    else if (ImGui::IsMouseHoveringRect(vHandlePosY - ImVec2(fHandleRadius, fHandleRadius) - vSecurity, vHandlePosY + ImVec2(fHandleRadius, fHandleRadius) + vSecurity) &&
        ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        ImVec2 const vCursorPos = ImGui::GetMousePos() - oRect.Min;

        *fValueY = fDeltaY - vCursorPos.y / (oRect.Max.y - oRect.Min.y) * fDeltaY + fMinY;

        bModified = true;
    }

    pDrawList->AddText(
        ImVec2(
            std::min(std::max(vCursorPos.x - vXSize.x * 0.5f, oRect.Min.x), oRect.Min.x + vSize.x - vXSize.x),
            oRect.Max.y + fCursorOff),
        uTextCol,
        pBufferX);
    pDrawList->AddText(
        ImVec2(oRect.Max.x + fCursorOff, std::min(std::max(vCursorPos.y - vYSize.y * 0.5f, oRect.Min.y),
            oRect.Min.y + vSize.y - vYSize.y)),
        uTextCol,
        pBufferY);
    ImGui::SetWindowFontScale(1.0f);

    // Borders::Right
    pDrawList->AddCircleFilled(ImVec2(oRect.Max.x, vCursorPos.y), 2.0f, uOrange, 3);
    // Handle Right::Y
    pDrawList->AddNgonFilled(ImVec2(oRect.Max.x + fHandleOffsetCoef * fCursorOff + vYSize.x, vCursorPos.y), fHandleRadius, uBlue, 4);
    if (fScaleY > fYLimit)
        pDrawList->AddLine(ImVec2(oRect.Max.x, oRect.Min.y), ImVec2(oRect.Max.x, vCursorPos.y - fCursorOff), uBlue, fBorderThickness);
    if (fScaleY < 1.0f - fYLimit)
        pDrawList->AddLine(ImVec2(oRect.Max.x, oRect.Max.y), ImVec2(oRect.Max.x, vCursorPos.y + fCursorOff), uBlue, fBorderThickness);
    // Borders::Top
    pDrawList->AddCircleFilled(ImVec2(vCursorPos.x, oRect.Min.y), 2.0f, uOrange, 3);
    if (fScaleX > fXLimit)
        pDrawList->AddLine(ImVec2(oRect.Min.x, oRect.Min.y), ImVec2(vCursorPos.x - fCursorOff, oRect.Min.y), uBlue, fBorderThickness);
    if (fScaleX < 1.0f - fXLimit)
        pDrawList->AddLine(ImVec2(oRect.Max.x, oRect.Min.y), ImVec2(vCursorPos.x + fCursorOff, oRect.Min.y), uBlue, fBorderThickness);
    // Borders::Left
    pDrawList->AddCircleFilled(ImVec2(oRect.Min.x, vCursorPos.y), 2.0f, uOrange, 3);
    if (fScaleY > fYLimit)
        pDrawList->AddLine(ImVec2(oRect.Min.x, oRect.Min.y), ImVec2(oRect.Min.x, vCursorPos.y - fCursorOff), uBlue, fBorderThickness);
    if (fScaleY < 1.0f - fYLimit)
        pDrawList->AddLine(ImVec2(oRect.Min.x, oRect.Max.y), ImVec2(oRect.Min.x, vCursorPos.y + fCursorOff), uBlue, fBorderThickness);
    // Borders::Bottom
    pDrawList->AddCircleFilled(ImVec2(vCursorPos.x, oRect.Max.y), 2.0f, uOrange, 3);
    // Handle Bottom::X
    pDrawList->AddNgonFilled(ImVec2(vCursorPos.x, oRect.Max.y + vXSize.y * 2.0f), fHandleRadius, uBlue, 4);
    if (fScaleX > fXLimit)
        pDrawList->AddLine(ImVec2(oRect.Min.x, oRect.Max.y), ImVec2(vCursorPos.x - fCursorOff, oRect.Max.y), uBlue, fBorderThickness);
    if (fScaleX < 1.0f - fXLimit)
        pDrawList->AddLine(ImVec2(oRect.Max.x, oRect.Max.y), ImVec2(vCursorPos.x + fCursorOff, oRect.Max.y), uBlue, fBorderThickness);

    ImGui::PopID();

    ImGui::Dummy(vHeightOffset);
    ImGui::Dummy(vHeightOffset);
    ImGui::Dummy(vSize);

    char pBufferID[64];
    snprintf(pBufferID, IM_ARRAYSIZE(pBufferID), "Values##%d", *(ImS32 const*)&iID);

    if (ImGui::CollapsingHeader(pBufferID))
    {
        float const fSpeedX = fDeltaX / 64.0f;
        float const fSpeedY = fDeltaY / 64.0f;

        char pBufferXID[64];
        snprintf(pBufferXID, IM_ARRAYSIZE(pBufferXID), "X##%d", *(ImS32 const*)&iID);
        char pBufferYID[64];
        snprintf(pBufferYID, IM_ARRAYSIZE(pBufferYID), "Y##%d", *(ImS32 const*)&iID);

        bModified |= ImGui::DragScalar(pBufferXID, ImGuiDataType_Float, fValueX, fSpeedX, &fMinX, &fMaxX);
        bModified |= ImGui::DragScalar(pBufferYID, ImGuiDataType_Float, fValueY, fSpeedY, &fMinY, &fMaxY);
    }

    return bModified;
}

float dot(ImVec2 a, ImVec2 b)
{
    return a.x * b.x + a.y * b.y;
}

float DistToSegmentSqr(ImVec2 v, ImVec2 w, ImVec2 p) 
{
    // Return minimum distance between line segment vw and point p

    ImVec2 d = v - w;

    const float l2 = d.x * d.x + d.y * d.y;  // i.e. |w-v|^2 -  avoid a sqrt
    if (l2 == 0.0)
    {
        d = p - v;
        return std::sqrtf(d.x * d.x + d.y * d.y);   // v == w case
    }
    // Consider the line extending the segment, parameterized as v + t (w - v).
    // We find projection of point p onto the line. 
    // It falls where t = [(p-v) . (w-v)] / |w-v|^2
    // We clamp t from [0,1] to handle points outside the segment vw.
    const float t = std::max(0.f, std::min(1.f, dot(p - v, w - v) / l2));
    const ImVec2 projection = v + (w - v) * t;  // Projection falls on the segment

    d = p - projection;
    return std::sqrtf(d.x * d.x + d.y * d.y);
}


bool SliderScalar3D(char const* pLabel, float* pValueX, float* pValueY, float* pValueZ, float const fMinX, float const fMaxX, float const fMinY, float const fMaxY, float const fMinZ, float const fMaxZ, float const fScale /*= 1.0f*/)
{
    assert(fMinX < fMaxX);
    assert(fMinY < fMaxY);
    assert(fMinZ < fMaxZ);

    ImGuiID const iID = ImGui::GetID(pLabel);

    ImVec2 const vSizeSubstract = ImGui::CalcTextSize(std::to_string(1.0f).c_str()) * 1.1f;

    float const vSizeFull = ImGui::GetWindowContentRegionWidth();
    float const fMinSize = (vSizeFull - vSizeSubstract.x * 0.5f) * fScale * 0.75f;
    ImVec2 const vSize(fMinSize, fMinSize);

    float const fHeightOffset = ImGui::GetTextLineHeight();
    ImVec2 const vHeightOffset(0.0f, fHeightOffset);

    ImVec2 vPos = GetCursorScreenPos();
    ImRect oRect(vPos + vHeightOffset, vPos + vSize + vHeightOffset);

    ImGui::TextUnformatted(pLabel);

    ImGui::PushID(iID);

    ImU32 const uFrameCol = ImGui::GetColorU32(ImGuiCol_FrameBg) | 0xFF000000;
    ImU32 const uFrameCol2 = ImGui::GetColorU32(ImGuiCol_FrameBgActive);

    float& fX = *pValueX;
    float& fY = *pValueY;
    float& fZ = *pValueZ;

    float const fDeltaX = fMaxX - fMinX;
    float const fDeltaY = fMaxY - fMinY;
    float const fDeltaZ = fMaxZ - fMinZ;

    ImVec2 const vOriginPos = ImGui::GetCursorScreenPos();

    ImDrawList* pDrawList = ImGui::GetWindowDrawList();

    float const fX3 = vSize.x / 3.0f;
    float const fY3 = vSize.y / 3.0f;

    ImVec2 const vStart = oRect.Min;

    ImVec2 aPositions[] = {
        ImVec2(vStart.x,			vStart.y + fX3),
        ImVec2(vStart.x,			vStart.y + vSize.y),
        ImVec2(vStart.x + 2.0f * fX3,	vStart.y + vSize.y),
        ImVec2(vStart.x + vSize.x,	vStart.y + 2.0f * fY3),
        ImVec2(vStart.x + vSize.x,	vStart.y),
        ImVec2(vStart.x + fX3,		vStart.y)
    };

    pDrawList->AddPolyline(&aPositions[0], 6, uFrameCol2, true, 1.0f);

    // Cube Shape
    pDrawList->AddLine(
        oRect.Min + ImVec2(0.0f, vSize.y),
        oRect.Min + ImVec2(fX3, 2.0f * fY3), uFrameCol2, 1.0f);
    pDrawList->AddLine(
        oRect.Min + ImVec2(fX3, 2.0f * fY3),
        oRect.Min + ImVec2(vSize.x, 2.0f * fY3), uFrameCol2, 1.0f);
    pDrawList->AddLine(
        oRect.Min + ImVec2(fX3, 0.0f),
        oRect.Min + ImVec2(fX3, 2.0f * fY3), uFrameCol2, 1.0f);

    ImGui::PopID();

    constexpr float fDragZOffsetX = 64.0f;

    ImRect oZDragRect(ImVec2(vStart.x + 2.0f * fX3 + fDragZOffsetX, vStart.y + 2.0f * fY3), ImVec2(vStart.x + vSize.x + fDragZOffsetX, vStart.y + vSize.y));

    ImVec2 const vMousePos = ImGui::GetMousePos();
    ImVec2 const vSecurity(15.0f, 15.0f);
    ImVec2 const vDragStart(oZDragRect.Min.x, oZDragRect.Max.y);
    ImVec2 const vDragEnd(oZDragRect.Max.x, oZDragRect.Min.y);
    bool bModified = false;
    if (ImGui::IsMouseHoveringRect(oZDragRect.Min - vSecurity, oZDragRect.Max + vSecurity) && ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        float dist = DistToSegmentSqr(vMousePos, vDragStart, vDragEnd);
        if (dist < 100.0f) // 100 is arbitrary threshold
        {
            ImVec2 d = vDragStart - vDragEnd;
            float const fMaxDist = std::sqrt(d.x * d.x + d.y * d.y);
            float const fDist = std::max(std::min(dist / fMaxDist, 1.0f), 0.0f);

            fZ = fDist * fDeltaZ * fDist + fMinZ;

            bModified = true;
        }
    }

    float const fScaleZ = (fZ - fMinZ) / fDeltaZ;

    ImVec2 const vRectStart(vStart.x, vStart.y + fX3);
    ImVec2 const vRectEnd(vStart.x + fX3, vStart.y);
    ImRect const oXYDrag((vRectEnd - vRectStart) * fScaleZ + vRectStart,
        (vRectEnd - vRectStart) * fScaleZ + vRectStart + ImVec2(2.0f * fX3, 2.0f * fY3));
    if (ImGui::IsMouseHoveringRect(oXYDrag.Min - vSecurity, oXYDrag.Max + vSecurity) && ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        ImVec2 const vLocalPos = ImGui::GetMousePos() - oXYDrag.Min;

        fX = vLocalPos.x / (oXYDrag.Max.x - oXYDrag.Min.x) * fDeltaX + fMinX;
        fY = fDeltaY - vLocalPos.y / (oXYDrag.Max.y - oXYDrag.Min.y) * fDeltaY + fMinY;

        bModified = true;
    }

    fX = std::min(std::max(fX, fMinX), fMaxX);
    fY = std::min(std::max(fY, fMinY), fMaxY);
    fZ = std::min(std::max(fZ, fMinZ), fMaxZ);

    float const fScaleX = (fX - fMinX) / fDeltaX;
    float const fScaleY = 1.0f - (fY - fMinY) / fDeltaY;

    ImVec4 const vBlue(70.0f / 255.0f, 102.0f / 255.0f, 230.0f / 255.0f, 1.0f);
    ImVec4 const vOrange(255.0f / 255.0f, 128.0f / 255.0f, 64.0f / 255.0f, 1.0f);

    ImS32 const uBlue = ImGui::GetColorU32(vBlue);
    ImS32 const uOrange = ImGui::GetColorU32(vOrange);

    constexpr float fBorderThickness = 2.0f; // TODO: move as Style
    constexpr float fLineThickness = 3.0f; // TODO: move as Style
    constexpr float fHandleRadius = 7.0f; // TODO: move as Style
    constexpr float fHandleOffsetCoef = 2.0f; // TODO: move as Style

    pDrawList->AddLine(
        vDragStart,
        vDragEnd, uFrameCol2, 1.0f);
    pDrawList->AddRectFilled(
        oXYDrag.Min, oXYDrag.Max, uFrameCol);

    constexpr float fCursorOff = 10.0f;
    float const fXLimit = fCursorOff / oXYDrag.GetWidth();
    float const fYLimit = fCursorOff / oXYDrag.GetHeight();
    float const fZLimit = fCursorOff / oXYDrag.GetWidth();

    char pBufferX[16];
    char pBufferY[16];
    char pBufferZ[16];
    snprintf(pBufferX, IM_ARRAYSIZE(pBufferX), "%.5f", *(float const*)&fX);
    snprintf(pBufferY, IM_ARRAYSIZE(pBufferY), "%.5f", *(float const*)&fY);
    snprintf(pBufferZ, IM_ARRAYSIZE(pBufferZ), "%.5f", *(float const*)&fZ);

    ImU32 const uTextCol = ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[ImGuiCol_Text]);

    ImVec2 const vCursorPos((oXYDrag.Max.x - oXYDrag.Min.x) * fScaleX + oXYDrag.Min.x, (oXYDrag.Max.y - oXYDrag.Min.y) * fScaleY + oXYDrag.Min.y);

    ImGui::SetWindowFontScale(0.75f);

    ImVec2 const vXSize = ImGui::CalcTextSize(pBufferX);
    ImVec2 const vYSize = ImGui::CalcTextSize(pBufferY);
    ImVec2 const vZSize = ImGui::CalcTextSize(pBufferZ);

    ImVec2 const vTextSlideXMin = oRect.Min + ImVec2(0.0f, vSize.y);
    ImVec2 const vTextSlideXMax = oRect.Min + ImVec2(2.0f * fX3, vSize.y);
    ImVec2 const vXTextPos((vTextSlideXMax - vTextSlideXMin) * fScaleX + vTextSlideXMin);

    ImVec2 const vTextSlideYMin = oRect.Min + ImVec2(vSize.x, 2.0f * fY3);
    ImVec2 const vTextSlideYMax = oRect.Min + ImVec2(vSize.x, 0.0f);
    ImVec2 const vYTextPos((vTextSlideYMax - vTextSlideYMin) * (1.0f - fScaleY) + vTextSlideYMin);

    ImVec2 const vTextSlideZMin = vDragStart + ImVec2(fCursorOff, fCursorOff);
    ImVec2 const vTextSlideZMax = vDragEnd + ImVec2(fCursorOff, fCursorOff);
    ImVec2 const vZTextPos((vTextSlideZMax - vTextSlideZMin) * fScaleZ + vTextSlideZMin);

    ImVec2 const vHandlePosX = vXTextPos + ImVec2(0.0f, vXSize.y + fHandleOffsetCoef * fCursorOff);
    ImVec2 const vHandlePosY = vYTextPos + ImVec2(vYSize.x + (fHandleOffsetCoef + 1.0f) * fCursorOff, 0.0f);

    if (ImGui::IsMouseHoveringRect(vHandlePosX - ImVec2(fHandleRadius, fHandleRadius) - vSecurity, vHandlePosX + ImVec2(fHandleRadius, fHandleRadius) + vSecurity) &&
        ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        float const fCursorPosX = ImGui::GetMousePos().x - vStart.x;

        fX = fDeltaX * fCursorPosX / (2.0f * fX3) + fMinX;

        bModified = true;
    }
    else if (ImGui::IsMouseHoveringRect(vHandlePosY - ImVec2(fHandleRadius, fHandleRadius) - vSecurity, vHandlePosY + ImVec2(fHandleRadius, fHandleRadius) + vSecurity) &&
        ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        float const fCursorPosY = ImGui::GetMousePos().y - vStart.y;

        fY = fDeltaY * (1.0f - fCursorPosY / (2.0f * fY3)) + fMinY;

        bModified = true;
    }

    pDrawList->AddText(
        ImVec2(
            std::min(std::max(vXTextPos.x - vXSize.x * 0.5f, vTextSlideXMin.x), vTextSlideXMax.x - vXSize.x),
            vXTextPos.y + fCursorOff),
        uTextCol,
        pBufferX);
    pDrawList->AddText(
        ImVec2(
            vYTextPos.x + fCursorOff,
            std::min(std::max(vYTextPos.y - vYSize.y * 0.5f, vTextSlideYMax.y), vTextSlideYMin.y - vYSize.y)),
        uTextCol,
        pBufferY);
    pDrawList->AddText(
        vZTextPos,
        uTextCol,
        pBufferZ);
    ImGui::SetWindowFontScale(1.0f);

    // Handles
    pDrawList->AddNgonFilled(vHandlePosX, fHandleRadius, uBlue, 4);
    pDrawList->AddNgonFilled(vHandlePosY, fHandleRadius, uBlue, 4);

    // Vertical Line
    if (fScaleY > 2.0f * fYLimit)
        pDrawList->AddLine(ImVec2(vCursorPos.x, oXYDrag.Min.y + fCursorOff), ImVec2(vCursorPos.x, vCursorPos.y - fCursorOff), uOrange, fLineThickness);
    if (fScaleY < 1.0f - 2.0f * fYLimit)
        pDrawList->AddLine(ImVec2(vCursorPos.x, oXYDrag.Max.y - fCursorOff), ImVec2(vCursorPos.x, vCursorPos.y + fCursorOff), uOrange, fLineThickness);

    // Horizontal Line
    if (fScaleX > 2.0f * fXLimit)
        pDrawList->AddLine(ImVec2(oXYDrag.Min.x + fCursorOff, vCursorPos.y), ImVec2(vCursorPos.x - fCursorOff, vCursorPos.y), uOrange, fLineThickness);
    if (fScaleX < 1.0f - 2.0f * fYLimit)
        pDrawList->AddLine(ImVec2(oXYDrag.Max.x - fCursorOff, vCursorPos.y), ImVec2(vCursorPos.x + fCursorOff, vCursorPos.y), uOrange, fLineThickness);

    // Line To Text
    // X
    if (fScaleZ > 2.0f * fZLimit)
        pDrawList->AddLine(
            ImVec2(vCursorPos.x - 0.5f * fCursorOff, oXYDrag.Max.y + 0.5f * fCursorOff),
            ImVec2(vXTextPos.x + 0.5f * fCursorOff, vXTextPos.y - 0.5f * fCursorOff), uOrange, fLineThickness
        );
    else
        pDrawList->AddLine(
            ImVec2(vCursorPos.x, oXYDrag.Max.y),
            ImVec2(vXTextPos.x, vXTextPos.y), uOrange, 1.0f
        );
    pDrawList->AddCircleFilled(vXTextPos, 2.0f, uOrange, 3);
    // Y
    if (fScaleZ < 1.0f - 2.0f * fZLimit)
        pDrawList->AddLine(
            ImVec2(oXYDrag.Max.x + 0.5f * fCursorOff, vCursorPos.y - 0.5f * fCursorOff),
            ImVec2(vYTextPos.x - 0.5f * fCursorOff, vYTextPos.y + 0.5f * fCursorOff), uOrange, fLineThickness
        );
    else
        pDrawList->AddLine(
            ImVec2(oXYDrag.Max.x, vCursorPos.y),
            ImVec2(vYTextPos.x, vYTextPos.y), uOrange, 1.0f
        );
    pDrawList->AddCircleFilled(vYTextPos, 2.0f, uOrange, 3);

    // Borders::Right
    pDrawList->AddCircleFilled(ImVec2(oXYDrag.Max.x, vCursorPos.y), 2.0f, uOrange, 3);
    if (fScaleY > fYLimit)
        pDrawList->AddLine(ImVec2(oXYDrag.Max.x, oXYDrag.Min.y), ImVec2(oXYDrag.Max.x, vCursorPos.y - fCursorOff), uBlue, fBorderThickness);
    if (fScaleY < 1.0f - fYLimit)
        pDrawList->AddLine(ImVec2(oXYDrag.Max.x, oXYDrag.Max.y), ImVec2(oXYDrag.Max.x, vCursorPos.y + fCursorOff), uBlue, fBorderThickness);
    // Borders::Top
    pDrawList->AddCircleFilled(ImVec2(vCursorPos.x, oXYDrag.Min.y), 2.0f, uOrange, 3);
    if (fScaleX > fXLimit)
        pDrawList->AddLine(ImVec2(oXYDrag.Min.x, oXYDrag.Min.y), ImVec2(vCursorPos.x - fCursorOff, oXYDrag.Min.y), uBlue, fBorderThickness);
    if (fScaleX < 1.0f - fXLimit)
        pDrawList->AddLine(ImVec2(oXYDrag.Max.x, oXYDrag.Min.y), ImVec2(vCursorPos.x + fCursorOff, oXYDrag.Min.y), uBlue, fBorderThickness);
    // Borders::Left
    pDrawList->AddCircleFilled(ImVec2(oXYDrag.Min.x, vCursorPos.y), 2.0f, uOrange, 3);
    if (fScaleY > fYLimit)
        pDrawList->AddLine(ImVec2(oXYDrag.Min.x, oXYDrag.Min.y), ImVec2(oXYDrag.Min.x, vCursorPos.y - fCursorOff), uBlue, fBorderThickness);
    if (fScaleY < 1.0f - fYLimit)
        pDrawList->AddLine(ImVec2(oXYDrag.Min.x, oXYDrag.Max.y), ImVec2(oXYDrag.Min.x, vCursorPos.y + fCursorOff), uBlue, fBorderThickness);
    // Borders::Bottom
    pDrawList->AddCircleFilled(ImVec2(vCursorPos.x, oXYDrag.Max.y), 2.0f, uOrange, 3);
    if (fScaleX > fXLimit)
        pDrawList->AddLine(ImVec2(oXYDrag.Min.x, oXYDrag.Max.y), ImVec2(vCursorPos.x - fCursorOff, oXYDrag.Max.y), uBlue, fBorderThickness);
    if (fScaleX < 1.0f - fXLimit)
        pDrawList->AddLine(ImVec2(oXYDrag.Max.x, oXYDrag.Max.y), ImVec2(vCursorPos.x + fCursorOff, oXYDrag.Max.y), uBlue, fBorderThickness);

    pDrawList->AddLine(
        oRect.Min + ImVec2(0.0f, fY3),
        oRect.Min + ImVec2(2.0f * fX3, fY3), uFrameCol2, 1.0f);
    pDrawList->AddLine(
        oRect.Min + ImVec2(2.0f * fX3, fY3),
        oRect.Min + ImVec2(vSize.x, 0.0f), uFrameCol2, 1.0f);
    pDrawList->AddLine(
        oRect.Min + ImVec2(2.0f * fX3, fY3),
        oRect.Min + ImVec2(2.0f * fX3, vSize.y), uFrameCol2, 1.0f);

    // Cursor
    pDrawList->AddCircleFilled(vCursorPos, 5.0f, uBlue, 16);
    // CursorZ
    pDrawList->AddNgonFilled((vDragEnd - vDragStart) * fScaleZ + vDragStart, fHandleRadius, uBlue, 4);

    ImGui::Dummy(vHeightOffset);
    ImGui::Dummy(vHeightOffset * 1.25f);
    ImGui::Dummy(vSize);

    char pBufferID[64];
    snprintf(pBufferID, IM_ARRAYSIZE(pBufferID), "Values##%d", *(ImS32 const*)&iID);

    if (ImGui::CollapsingHeader(pBufferID))
    {
        float const fMoveDeltaX = fDeltaX / 64.0f; // Arbitrary
        float const fMoveDeltaY = fDeltaY / 64.0f; // Arbitrary
        float const fMoveDeltaZ = fDeltaZ / 64.0f; // Arbitrary

        char pBufferXID[64];
        snprintf(pBufferXID, IM_ARRAYSIZE(pBufferXID), "X##%d", *(ImS32 const*)&iID);
        char pBufferYID[64];
        snprintf(pBufferYID, IM_ARRAYSIZE(pBufferYID), "Y##%d", *(ImS32 const*)&iID);
        char pBufferZID[64];
        snprintf(pBufferZID, IM_ARRAYSIZE(pBufferZID), "Z##%d", *(ImS32 const*)&iID);

        bModified |= ImGui::DragScalar(pBufferXID, ImGuiDataType_Float, &fX, fMoveDeltaX, &fMinX, &fMaxX);
        bModified |= ImGui::DragScalar(pBufferYID, ImGuiDataType_Float, &fY, fMoveDeltaY, &fMinY, &fMaxY);
        bModified |= ImGui::DragScalar(pBufferZID, ImGuiDataType_Float, &fZ, fMoveDeltaZ, &fMinZ, &fMaxZ);
    }

    return bModified;
}


bool InputVec2(char const* pLabel, ImVec2* pValue, ImVec2 const vMinValue, ImVec2 const vMaxValue, float const fScale /*= 1.0f*/)
{
    return SliderScalar2D(pLabel, &pValue->x, &pValue->y, vMinValue.x, vMaxValue.x, vMinValue.y, vMaxValue.y, fScale);
}

bool InputVec3(char const* pLabel, ImVec4* pValue, ImVec4 const vMinValue, ImVec4 const vMaxValue, float const fScale /*= 1.0f*/)
{
    return SliderScalar3D(pLabel, &pValue->x, &pValue->y, &pValue->z, vMinValue.x, vMaxValue.x, vMinValue.y, vMaxValue.y, vMinValue.z, vMaxValue.z, fScale);
}
