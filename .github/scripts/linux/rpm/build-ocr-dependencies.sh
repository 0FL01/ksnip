#!/usr/bin/env bash

set -euo pipefail

if [ "$#" -lt 2 ] || [ "$#" -gt 3 ]; then
	echo "Usage: $0 <source-cache> <build-root> [jobs]" >&2
	exit 2
fi

source_cache=$(readlink -f "$1")
build_root=$2
jobs=${3:-2}
script_dir=$(dirname "$(readlink -f "$0")")
lock_file="$script_dir/ocr-sources.lock"
project_root=$(readlink -f "$script_dir/../../../..")

without_lto_flags()
{
	local filtered=()
	local flag
	for flag in $1; do
		case "$flag" in
			-flto|-flto=*|-ffat-lto-objects|-fno-fat-lto-objects) ;;
			*) filtered+=("$flag") ;;
		esac
	done
	printf '%s' "${filtered[*]}"
}

bash "$script_dir/fetch-ocr-sources.sh" "$source_cache" --offline

mkdir -p "$build_root/src" "$build_root/mirror"
tar -xf "$source_cache/onnxruntime-v1.27.0.tar.gz" -C "$build_root/src"
tar -xf "$source_cache/opencv-4.12.0.tar.gz" -C "$build_root/src"

while read -r sha256 filename url; do
	case "$sha256" in
		""|\#*) continue ;;
	esac
	case "$filename" in
		onnxruntime-*|opencv-*) continue ;;
	esac
	relative_url=${url#https://}
	install -Dpm0644 "$source_cache/$filename" "$build_root/mirror/$relative_url"
done < "$lock_file"

export CFLAGS="$(without_lto_flags "${CFLAGS:-}") -fno-lto -ffunction-sections -fdata-sections"
export CXXFLAGS="$(without_lto_flags "${CXXFLAGS:-}") -fno-lto -ffunction-sections -fdata-sections"
export LDFLAGS="$(without_lto_flags "${LDFLAGS:-}") -fno-lto"

opencv_source="$build_root/src/opencv-4.12.0"
opencv_build="$build_root/opencv-build"
opencv_prefix="$build_root/opencv-prefix"
cmake -S "$opencv_source" -B "$opencv_build" -G Ninja \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF \
	-DCMAKE_INSTALL_PREFIX="$opencv_prefix" \
	-DBUILD_LIST=core,imgproc \
	-DBUILD_SHARED_LIBS=OFF \
	-DBUILD_DOCS=OFF \
	-DBUILD_EXAMPLES=OFF \
	-DBUILD_JAVA=OFF \
	-DBUILD_opencv_apps=OFF \
	-DBUILD_PACKAGE=OFF \
	-DBUILD_PERF_TESTS=OFF \
	-DBUILD_TESTS=OFF \
	-DBUILD_ZLIB=OFF \
	-DCPU_BASELINE=SSE3 \
	-DCPU_DISPATCH= \
	-DENABLE_CCACHE=ON \
	-DENABLE_PIC=ON \
	-DENABLE_PRECOMPILED_HEADERS=OFF \
	-DOPENCV_GENERATE_PKGCONFIG=OFF \
	-DWITH_1394=OFF \
	-DWITH_ADE=OFF \
	-DWITH_AVIF=OFF \
	-DWITH_EIGEN=OFF \
	-DWITH_FFMPEG=OFF \
	-DWITH_FLATBUFFERS=OFF \
	-DWITH_GSTREAMER=OFF \
	-DWITH_GTK=OFF \
	-DWITH_IPP=OFF \
	-DWITH_ITT=OFF \
	-DWITH_JASPER=OFF \
	-DWITH_JPEG=OFF \
	-DWITH_LAPACK=OFF \
	-DWITH_OPENCL=OFF \
	-DWITH_OPENEXR=OFF \
	-DWITH_OPENJPEG=OFF \
	-DWITH_OPENMP=OFF \
	-DWITH_PNG=OFF \
	-DWITH_PROTOBUF=OFF \
	-DWITH_QT=OFF \
	-DWITH_QUIRC=OFF \
	-DWITH_SPNG=OFF \
	-DWITH_TBB=OFF \
	-DWITH_TIFF=OFF \
	-DWITH_V4L=OFF \
	-DWITH_VA=OFF \
	-DWITH_VA_INTEL=OFF \
	-DWITH_WEBP=OFF
cmake --build "$opencv_build" --parallel "$jobs"
cmake --install "$opencv_build"

ort_source="$build_root/src/onnxruntime-1.27.0"
python_bin=${KSNIP_OCR_PYTHON:-/usr/bin/python3}
if [ ! -x "$python_bin" ]; then
	python_bin=$(command -v python3)
fi
env -u CMAKE_PREFIX_PATH -u CONDA_PREFIX -u PYTHONPATH "$python_bin" "$ort_source/tools/ci_build/build.py" \
	--build_dir "$build_root/ort" \
	--config Release \
	--parallel "$jobs" \
	--cmake_generator Ninja \
	--cmake_deps_mirror_dir "$build_root/mirror" \
	--compile_no_warning_as_error \
	--disable_generation_ops \
	--skip_pip_install \
	--skip_submodule_sync \
	--skip_tests \
	--include_ops_by_config "$project_root/src/gui/ocr/required_operators.config" \
	--disable_contrib_ops \
	--disable_ml_ops \
	--disable_rtti \
	--disable_types float4 float8 optional sparsetensor string \
	--no_kleidiai \
	--cmake_extra_defines \
		CMAKE_POSITION_INDEPENDENT_CODE=ON \
		onnxruntime_BUILD_FOR_NATIVE_MACHINE=OFF \
		onnxruntime_BUILD_UNIT_TESTS=OFF \
		onnxruntime_ENABLE_LTO=OFF \
		onnxruntime_USE_SVE=OFF

test -f "$opencv_prefix/lib64/libopencv_core.a"
test -f "$opencv_prefix/lib64/libopencv_imgproc.a"
test -f "$build_root/ort/Release/libonnxruntime_session.a"
