// SPDX-License-Identifier: MPL-2.0
#include "../app/src/main/cpp/skyline/soc/host1x/classes/vic_yuv420_output.h"
#include <cassert>
#include <cstdint>
#include <vector>

using namespace skyline::soc::host1x::vic_output;

int main() {
    constexpr std::uint32_t stride=24, slice=24, cropLeft=2, cropTop=4;
    constexpr std::uint32_t width=16, height=16;
    constexpr std::size_t yBytes=stride*slice;
    // 24x24 coded backing with a 16x16 visible crop; UV is NV12.
    std::vector<std::uint8_t> nv12(yBytes + stride*(slice/2U), 128);
    for(std::uint32_t y=0;y<slice;++y)
        for(std::uint32_t x=0;x<stride;++x)
            nv12[y*stride+x]=static_cast<std::uint8_t>((y*stride+x)%200 + 16);
    for(std::size_t i=yBytes;i<nv12.size();i+=2) {
        nv12[i]=110;
        nv12[i+1]=145;
    }
    Output output;
    Surface yuv{width,height,0,GuestYuv420,0};
    assert(PackLinearNv12(nv12,width,height,stride,slice,
                          AndroidYuvSemiplanar,yuv,output,cropLeft,cropTop));
    constexpr std::uint32_t pitch=256;
    assert(output.luma.size()==pitch*height);
    assert(output.chroma.size()==pitch*(height/2U));
    assert(output.luma[0]==nv12[cropTop*stride+cropLeft]);
    assert(output.luma[pitch]==nv12[(cropTop+1)*stride+cropLeft]);
    assert(output.luma[width-1]==nv12[cropTop*stride+cropLeft+width-1]);
    assert(output.chroma[0]==nv12[yBytes+(cropTop/2)*stride+cropLeft]);
    assert(output.chroma[1]==145);

    Surface bgra{width,height,0,GuestBgra8,0};
    Output rgba;
    assert(PackLinearRgba(nv12,width,height,stride,slice,
                          AndroidYuvSemiplanar,bgra,rgba,cropLeft,cropTop));
    const auto color=YuvToRgb(nv12[cropTop*stride+cropLeft],110,145);
    assert(rgba.luma[0]==color.b && rgba.luma[1]==color.g &&
           rgba.luma[2]==color.r && rgba.luma[3]==255);
    assert(rgba.chroma.empty());

    // Reject mismatched visible extents, odd 4:2:0 crop and overflow; do not
    // overwrite the previously valid output on an unsupported frame.
    const auto saved=output.luma;
    assert(!PackLinearNv12(nv12,width,height,stride,slice,
                           AndroidYuvSemiplanar,yuv,output,3,cropTop));
    assert(!PackLinearNv12(nv12,width,height,stride,slice,
                           AndroidYuvSemiplanar,yuv,output,cropLeft,5));
    assert(!PackLinearNv12(nv12,width,height,stride,slice,
                           AndroidYuvSemiplanar,yuv,output,10,cropTop));
    assert(!PackLinearNv12(nv12,width,height,stride,slice,
                           AndroidYuvSemiplanar,yuv,output,cropLeft,10));
    assert(!PackLinearNv12(nv12,width,height,stride,slice,
                           AndroidYuvSemiplanar,Surface{20,16,0,GuestYuv420,0},
                           output,cropLeft,cropTop));
    assert(output.luma==saved);

    // Planar U/V have half-resolution crop coordinates and independent planes.
    std::vector<std::uint8_t> planar(yBytes+2U*(stride/2U)*(slice/2U),0);
    for(std::size_t i=0;i<yBytes;++i)planar[i]=nv12[i];
    const auto planeSize=(stride/2U)*(slice/2U);
    for(std::size_t i=yBytes;i<yBytes+planeSize;++i)planar[i]=110;
    for(std::size_t i=yBytes+planeSize;i<planar.size();++i)planar[i]=145;
    Output planarResult;
    assert(PackLinearNv12(planar,width,height,stride,slice,
                          AndroidYuvPlanar,yuv,planarResult,cropLeft,cropTop));
    assert(planarResult.chroma[0]==110 && planarResult.chroma[1]==145);
}
