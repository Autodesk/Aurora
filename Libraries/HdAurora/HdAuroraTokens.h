// Copyright 2023 Autodesk, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#pragma once

namespace HdAuroraTokens
{

//**************************************************************************/
// Render Setting Tokens
//**************************************************************************/

/// The maximum path tracing trace depth.
static const TfToken kTraceDepth("aurora:trace_depth");

/// The maximum number of path tracing samples per-pixel.
static const TfToken kMaxSamples("aurora:max_samples");

/// The current number of per-pixel samples rendered, as an output value.
static const TfToken kCurrentSamples("aurora:current_samples");

/// Whether to restart rendering, from the first sample.
static const TfToken kIsRestartEnabled("aurora:is_restart_enabled");

/// Whether to enable denoising.
static const TfToken kIsDenoisingEnabled("aurora:is_denoising_enabled");

/// Whether the alpha channel is enabled for output.
static const TfToken kIsAlphaEnabled("aurora:is_alpha_enabled");

/// The path to the background image file.
static const TfToken kBackgroundImage("aurora:background_image");

/// The top and bottom colors of the background, for a vertical gradient.
static const TfToken kBackgroundColors("aurora:background_colors");

/// Whether to use the environment image (from the dome light) as the background image.
static const TfToken kUseEnvironmentImageAsBackground("aurora:use_environment_image_as_background");

/// Whether to perform inverse tone mapping on the background image.
static const TfToken kIsInverseToneMappingEnabled("aurora:is_inverse_tone_mapping_enabled");

/// The exposure to use for inverse tone mapping on the background image.
static const TfToken kInverseToneMappingExposure("aurora:inverse_tone_mapping_exposure");

/// Whether to use a shared handle for renderer output.
static const TfToken kIsSharedHandleEnabled("aurora:is_shared_handle_enabled");

/// Whether the shared handle for renderer output is from DirectX, as an output value.
static const TfToken kIsSharedHandleDirectX("aurora:is_shared_handle_directx");

/// Whether the shared handle for renderer output is from OpenGL, as an output value.
static const TfToken kIsSharedHandleOpenGL("aurora:is_shared_handle_opengl");

/// The settings for the global ground plane, as defined in the Aurora API.
static const TfToken kGroundPlaneSettings("aurora:ground_plane_settings");

/// Whether to flip the output image.
static const TfToken kFlipYOutput("aurora:flip_y_output");

/// Whether to flip the output image.
static const TfToken kFlipLoadedImageY("aurora:flip_loaded_image_y");

//**************************************************************************/
// Material Tokens
//**************************************************************************/

/// The path to a MaterialX document file for a material.
static const TfToken kMaterialXFilePath("aurora:materialx_file_path");

/// The MaterialX document for a material, as XML in memory.
static const TfToken kMaterialXDocument("aurora:materialx_document");

/// If true the opacity UsdPreviewSurface parameter is mapped to transmission in Standard Surface.
/// This matches the definition more closely (defaults to true.)
static const TfToken kMapMaterialOpacityToTransmission(
    "aurora:map_material_opacity_to_transmission");

//**************************************************************************/
// Mesh Tokens
//**************************************************************************/

/// A vector of SdfPath objects providing material IDs of material layers for a mesh.
///
/// Layers begin at the inner-most layer (closest to the base layer) and proceed outward.
static const TfToken kMeshMaterialLayers("aurora:mesh_material_layers");

/// A vector of VtToken objects providing primvar names of geometry layer UVs for a mesh.
///
/// Layers begin at the inner-most layer (closest to the base layer) and proceed outward.
static const TfToken kMeshGeometryLayerUVs("aurora:mesh_geometry_layer_uvs");

}; // namespace HdAuroraTokens
