void setupMaterial_55c0082a3bf2c52a(
	Material_55c0082a3bf2c52a material,
	sampler2D basecolor_bitmap_image_parameter,
	sampler2D opacity_bitmap_image_parameter,
	int distance_unit,
	out float base,
	out vec3 base_color,
	out float metalness,
	out float specular,
	out float specular_roughness,
	out float specular_IOR,
	out vec3 opacity,
	out bool thin_walled)
{
	// Graph input TestMaskWithChromeKeyDecal/base
	//{
float TestMaskWithChromeKeyDecal_base//};
 = material.base;
	base = TestMaskWithChromeKeyDecal_base;// Output connection
	//Temp input variables for basecolor_bitmap 
	vec3 nodeOutTmp_basecolor_bitmap_out; //Temp output variable for out 
	sampler2D nodeTmp_basecolor_bitmap_file; //Temp input variable for file 
	vec2 nodeTmp_basecolor_bitmap_realworld_offset; //Temp input variable for realworld_offset 
	vec2 nodeTmp_basecolor_bitmap_realworld_scale; //Temp input variable for realworld_scale 
	vec2 nodeTmp_basecolor_bitmap_uv_offset; //Temp input variable for uv_offset 
	vec2 nodeTmp_basecolor_bitmap_uv_scale; //Temp input variable for uv_scale 
	float nodeTmp_basecolor_bitmap_rotation_angle; //Temp input variable for rotation_angle 
	float nodeTmp_basecolor_bitmap_rgbamount; //Temp input variable for rgbamount 
	bool nodeTmp_basecolor_bitmap_invert; //Temp input variable for invert 
	int nodeTmp_basecolor_bitmap_uaddressmode; //Temp input variable for uaddressmode 
	int nodeTmp_basecolor_bitmap_vaddressmode; //Temp input variable for vaddressmode 
	int nodeTmp_basecolor_bitmap_uv_index; //Temp input variable for uv_index 
	// Graph input TestMaskWithChromeKeyDecal_nodegraph/basecolor_bitmap/file
	//{
sampler2D basecolor_bitmap_file1//};
 = basecolor_bitmap_image_parameter;
	nodeTmp_basecolor_bitmap_file = basecolor_bitmap_file1;// Output connection
	//Temp input variables for basecolor_bitmap_realworld_offset_unit 
	vec2 nodeOutTmp_basecolor_bitmap_realworld_offset_unit_out; //Temp output variable for out 
	vec2 nodeTmp_basecolor_bitmap_realworld_offset_unit_in; //Temp input variable for in 
	int nodeTmp_basecolor_bitmap_realworld_offset_unit_unit_from; //Temp input variable for unit_from 
	int nodeTmp_basecolor_bitmap_realworld_offset_unit_unit_to; //Temp input variable for unit_to 
	// Graph input TestMaskWithChromeKeyDecal_nodegraph/basecolor_bitmap/realworld_offset
	//{
vec2 basecolor_bitmap_realworld_offset_unit_in1//};
 = material.basecolor_bitmap_realworld_offset;
	nodeTmp_basecolor_bitmap_realworld_offset_unit_in = basecolor_bitmap_realworld_offset_unit_in1;// Output connection
	// Graph input 
	//{
int basecolor_bitmap_realworld_offset_unit_unit_from1 = 3//};
;
	nodeTmp_basecolor_bitmap_realworld_offset_unit_unit_from = basecolor_bitmap_realworld_offset_unit_unit_from1;// Output connection
	// Graph input 
	//{
int basecolor_bitmap_realworld_offset_unit_unit_to//};
 = distance_unit;
	nodeTmp_basecolor_bitmap_realworld_offset_unit_unit_to = basecolor_bitmap_realworld_offset_unit_unit_to;// Output connection
	// Graph input function call realworld_offset (See definition IM_distance_unit_vector2_genglsl)
{
	//{
    vec2 basecolor_bitmap_realworld_offset_unit_out = nodeTmp_basecolor_bitmap_realworld_offset_unit_in * mx_distance_unit_ratio(nodeTmp_basecolor_bitmap_realworld_offset_unit_unit_from, nodeTmp_basecolor_bitmap_realworld_offset_unit_unit_to);
//};
	nodeOutTmp_basecolor_bitmap_realworld_offset_unit_out = basecolor_bitmap_realworld_offset_unit_out;// Output connection
	nodeTmp_basecolor_bitmap_realworld_offset = basecolor_bitmap_realworld_offset_unit_out;// Output connection
}
	//Temp input variables for basecolor_bitmap_realworld_scale_unit 
	vec2 nodeOutTmp_basecolor_bitmap_realworld_scale_unit_out; //Temp output variable for out 
	vec2 nodeTmp_basecolor_bitmap_realworld_scale_unit_in; //Temp input variable for in 
	int nodeTmp_basecolor_bitmap_realworld_scale_unit_unit_from; //Temp input variable for unit_from 
	int nodeTmp_basecolor_bitmap_realworld_scale_unit_unit_to; //Temp input variable for unit_to 
	// Graph input TestMaskWithChromeKeyDecal_nodegraph/basecolor_bitmap/realworld_scale
	//{
vec2 basecolor_bitmap_realworld_scale_unit_in1//};
 = material.basecolor_bitmap_realworld_scale;
	nodeTmp_basecolor_bitmap_realworld_scale_unit_in = basecolor_bitmap_realworld_scale_unit_in1;// Output connection
	// Graph input 
	//{
int basecolor_bitmap_realworld_scale_unit_unit_from1 = 3//};
;
	nodeTmp_basecolor_bitmap_realworld_scale_unit_unit_from = basecolor_bitmap_realworld_scale_unit_unit_from1;// Output connection
	// Graph input 
	//{
int basecolor_bitmap_realworld_scale_unit_unit_to//};
 = distance_unit;
	nodeTmp_basecolor_bitmap_realworld_scale_unit_unit_to = basecolor_bitmap_realworld_scale_unit_unit_to;// Output connection
	// Graph input function call realworld_scale (See definition IM_distance_unit_vector2_genglsl)
{
	//{
    vec2 basecolor_bitmap_realworld_scale_unit_out = nodeTmp_basecolor_bitmap_realworld_scale_unit_in * mx_distance_unit_ratio(nodeTmp_basecolor_bitmap_realworld_scale_unit_unit_from, nodeTmp_basecolor_bitmap_realworld_scale_unit_unit_to);
//};
	nodeOutTmp_basecolor_bitmap_realworld_scale_unit_out = basecolor_bitmap_realworld_scale_unit_out;// Output connection
	nodeTmp_basecolor_bitmap_realworld_scale = basecolor_bitmap_realworld_scale_unit_out;// Output connection
}
	//Temp input variables for basecolor_bitmap_uv_offset_unit 
	vec2 nodeOutTmp_basecolor_bitmap_uv_offset_unit_out; //Temp output variable for out 
	vec2 nodeTmp_basecolor_bitmap_uv_offset_unit_in; //Temp input variable for in 
	int nodeTmp_basecolor_bitmap_uv_offset_unit_unit_from; //Temp input variable for unit_from 
	int nodeTmp_basecolor_bitmap_uv_offset_unit_unit_to; //Temp input variable for unit_to 
	// Graph input TestMaskWithChromeKeyDecal_nodegraph/basecolor_bitmap/uv_offset
	//{
vec2 basecolor_bitmap_uv_offset_unit_in1//};
 = material.basecolor_bitmap_uv_offset;
	nodeTmp_basecolor_bitmap_uv_offset_unit_in = basecolor_bitmap_uv_offset_unit_in1;// Output connection
	// Graph input 
	//{
int basecolor_bitmap_uv_offset_unit_unit_from1 = 3//};
;
	nodeTmp_basecolor_bitmap_uv_offset_unit_unit_from = basecolor_bitmap_uv_offset_unit_unit_from1;// Output connection
	// Graph input 
	//{
int basecolor_bitmap_uv_offset_unit_unit_to//};
 = distance_unit;
	nodeTmp_basecolor_bitmap_uv_offset_unit_unit_to = basecolor_bitmap_uv_offset_unit_unit_to;// Output connection
	// Graph input function call uv_offset (See definition IM_distance_unit_vector2_genglsl)
{
	//{
    vec2 basecolor_bitmap_uv_offset_unit_out = nodeTmp_basecolor_bitmap_uv_offset_unit_in * mx_distance_unit_ratio(nodeTmp_basecolor_bitmap_uv_offset_unit_unit_from, nodeTmp_basecolor_bitmap_uv_offset_unit_unit_to);
//};
	nodeOutTmp_basecolor_bitmap_uv_offset_unit_out = basecolor_bitmap_uv_offset_unit_out;// Output connection
	nodeTmp_basecolor_bitmap_uv_offset = basecolor_bitmap_uv_offset_unit_out;// Output connection
}
	//Temp input variables for basecolor_bitmap_uv_scale_unit 
	vec2 nodeOutTmp_basecolor_bitmap_uv_scale_unit_out; //Temp output variable for out 
	vec2 nodeTmp_basecolor_bitmap_uv_scale_unit_in; //Temp input variable for in 
	int nodeTmp_basecolor_bitmap_uv_scale_unit_unit_from; //Temp input variable for unit_from 
	int nodeTmp_basecolor_bitmap_uv_scale_unit_unit_to; //Temp input variable for unit_to 
	// Graph input TestMaskWithChromeKeyDecal_nodegraph/basecolor_bitmap/uv_scale
	//{
vec2 basecolor_bitmap_uv_scale_unit_in1//};
 = material.basecolor_bitmap_uv_scale;
	nodeTmp_basecolor_bitmap_uv_scale_unit_in = basecolor_bitmap_uv_scale_unit_in1;// Output connection
	// Graph input 
	//{
int basecolor_bitmap_uv_scale_unit_unit_from1 = 3//};
;
	nodeTmp_basecolor_bitmap_uv_scale_unit_unit_from = basecolor_bitmap_uv_scale_unit_unit_from1;// Output connection
	// Graph input 
	//{
int basecolor_bitmap_uv_scale_unit_unit_to//};
 = distance_unit;
	nodeTmp_basecolor_bitmap_uv_scale_unit_unit_to = basecolor_bitmap_uv_scale_unit_unit_to;// Output connection
	// Graph input function call uv_scale (See definition IM_distance_unit_vector2_genglsl)
{
	//{
    vec2 basecolor_bitmap_uv_scale_unit_out = nodeTmp_basecolor_bitmap_uv_scale_unit_in * mx_distance_unit_ratio(nodeTmp_basecolor_bitmap_uv_scale_unit_unit_from, nodeTmp_basecolor_bitmap_uv_scale_unit_unit_to);
//};
	nodeOutTmp_basecolor_bitmap_uv_scale_unit_out = basecolor_bitmap_uv_scale_unit_out;// Output connection
	nodeTmp_basecolor_bitmap_uv_scale = basecolor_bitmap_uv_scale_unit_out;// Output connection
}
	// Graph input TestMaskWithChromeKeyDecal_nodegraph/basecolor_bitmap/rotation_angle
	//{
float basecolor_bitmap_rotation_angle1//};
 = material.basecolor_bitmap_rotation_angle;
	nodeTmp_basecolor_bitmap_rotation_angle = basecolor_bitmap_rotation_angle1;// Output connection
	// Graph input TestMaskWithChromeKeyDecal_nodegraph/basecolor_bitmap/rgbamount
	//{
float basecolor_bitmap_rgbamount1 = 1//};
;
	nodeTmp_basecolor_bitmap_rgbamount = basecolor_bitmap_rgbamount1;// Output connection
	// Graph input TestMaskWithChromeKeyDecal_nodegraph/basecolor_bitmap/invert
	//{
bool basecolor_bitmap_invert1 = false//};
;
	nodeTmp_basecolor_bitmap_invert = basecolor_bitmap_invert1;// Output connection
	// Graph input TestMaskWithChromeKeyDecal_nodegraph/basecolor_bitmap/uaddressmode
	//{
int basecolor_bitmap_uaddressmode1 = 1//};
;
	nodeTmp_basecolor_bitmap_uaddressmode = basecolor_bitmap_uaddressmode1;// Output connection
	// Graph input TestMaskWithChromeKeyDecal_nodegraph/basecolor_bitmap/vaddressmode
	//{
int basecolor_bitmap_vaddressmode1 = 1//};
;
	nodeTmp_basecolor_bitmap_vaddressmode = basecolor_bitmap_vaddressmode1;// Output connection
	// Graph input TestMaskWithChromeKeyDecal_nodegraph/basecolor_bitmap/uv_index
	//{
int basecolor_bitmap_uv_index1 = 0//};
;
	nodeTmp_basecolor_bitmap_uv_index = basecolor_bitmap_uv_index1;// Output connection
	// Graph input function call base_color (See definition adsk:NG_adsk_bitmap_color3)
{
	//{
    vec3 basecolor_bitmap_out = vec3(0.0);
    adsk_NG_adsk_bitmap_color3(nodeTmp_basecolor_bitmap_file, nodeTmp_basecolor_bitmap_realworld_offset, nodeTmp_basecolor_bitmap_realworld_scale, nodeTmp_basecolor_bitmap_uv_offset, nodeTmp_basecolor_bitmap_uv_scale, nodeTmp_basecolor_bitmap_rotation_angle, nodeTmp_basecolor_bitmap_rgbamount, nodeTmp_basecolor_bitmap_invert, nodeTmp_basecolor_bitmap_uaddressmode, nodeTmp_basecolor_bitmap_vaddressmode, nodeTmp_basecolor_bitmap_uv_index, basecolor_bitmap_out);
//};
	nodeOutTmp_basecolor_bitmap_out = basecolor_bitmap_out;// Output connection
	base_color = basecolor_bitmap_out;// Output connection
}
	// Graph input TestMaskWithChromeKeyDecal/metalness
	//{
float TestMaskWithChromeKeyDecal_metalness//};
 = material.metalness;
	metalness = TestMaskWithChromeKeyDecal_metalness;// Output connection
	// Graph input TestMaskWithChromeKeyDecal/specular
	//{
float TestMaskWithChromeKeyDecal_specular//};
 = material.specular;
	specular = TestMaskWithChromeKeyDecal_specular;// Output connection
	// Graph input TestMaskWithChromeKeyDecal/specular_roughness
	//{
float TestMaskWithChromeKeyDecal_specular_roughness//};
 = material.specular_roughness;
	specular_roughness = TestMaskWithChromeKeyDecal_specular_roughness;// Output connection
	// Graph input TestMaskWithChromeKeyDecal/specular_IOR
	//{
float TestMaskWithChromeKeyDecal_specular_IOR//};
 = material.specular_IOR;
	specular_IOR = TestMaskWithChromeKeyDecal_specular_IOR;// Output connection
	//Temp input variables for convert_to_opacity_white_black 
	vec3 nodeOutTmp_convert_to_opacity_white_black_out; //Temp output variable for out 
	float nodeTmp_convert_to_opacity_white_black_value1; //Temp input variable for value1 
	float nodeTmp_convert_to_opacity_white_black_value2; //Temp input variable for value2 
	vec3 nodeTmp_convert_to_opacity_white_black_in1; //Temp input variable for in1 
	vec3 nodeTmp_convert_to_opacity_white_black_in2; //Temp input variable for in2 
	//Temp input variables for magnitude_vector3 
	float nodeOutTmp_magnitude_vector3_out; //Temp output variable for out 
	vec3 nodeTmp_magnitude_vector3_in; //Temp input variable for in 
	//Temp input variables for convert_color3_to_vector3 
	vec3 nodeOutTmp_convert_color3_to_vector3_out; //Temp output variable for out 
	vec3 nodeTmp_convert_color3_to_vector3_in; //Temp input variable for in 
	//Temp input variables for difference_image_with_chromekey 
	vec3 nodeOutTmp_difference_image_with_chromekey_out; //Temp output variable for out 
	vec3 nodeTmp_difference_image_with_chromekey_fg; //Temp input variable for fg 
	vec3 nodeTmp_difference_image_with_chromekey_bg; //Temp input variable for bg 
	float nodeTmp_difference_image_with_chromekey_mix; //Temp input variable for mix 
	//Temp input variables for opacity_bitmap 
	vec3 nodeOutTmp_opacity_bitmap_out; //Temp output variable for out 
	sampler2D nodeTmp_opacity_bitmap_file; //Temp input variable for file 
	vec2 nodeTmp_opacity_bitmap_realworld_offset; //Temp input variable for realworld_offset 
	vec2 nodeTmp_opacity_bitmap_realworld_scale; //Temp input variable for realworld_scale 
	vec2 nodeTmp_opacity_bitmap_uv_offset; //Temp input variable for uv_offset 
	vec2 nodeTmp_opacity_bitmap_uv_scale; //Temp input variable for uv_scale 
	float nodeTmp_opacity_bitmap_rotation_angle; //Temp input variable for rotation_angle 
	float nodeTmp_opacity_bitmap_rgbamount; //Temp input variable for rgbamount 
	bool nodeTmp_opacity_bitmap_invert; //Temp input variable for invert 
	int nodeTmp_opacity_bitmap_uaddressmode; //Temp input variable for uaddressmode 
	int nodeTmp_opacity_bitmap_vaddressmode; //Temp input variable for vaddressmode 
	int nodeTmp_opacity_bitmap_uv_index; //Temp input variable for uv_index 
	// Graph input TestMaskWithChromeKeyDecal_nodegraph/opacity_bitmap/file
	//{
sampler2D opacity_bitmap_file1//};
 = opacity_bitmap_image_parameter;
	nodeTmp_opacity_bitmap_file = opacity_bitmap_file1;// Output connection
	//Temp input variables for opacity_bitmap_realworld_offset_unit 
	vec2 nodeOutTmp_opacity_bitmap_realworld_offset_unit_out; //Temp output variable for out 
	vec2 nodeTmp_opacity_bitmap_realworld_offset_unit_in; //Temp input variable for in 
	int nodeTmp_opacity_bitmap_realworld_offset_unit_unit_from; //Temp input variable for unit_from 
	int nodeTmp_opacity_bitmap_realworld_offset_unit_unit_to; //Temp input variable for unit_to 
	// Graph input TestMaskWithChromeKeyDecal_nodegraph/opacity_bitmap/realworld_offset
	//{
vec2 opacity_bitmap_realworld_offset_unit_in1//};
 = material.opacity_bitmap_realworld_offset;
	nodeTmp_opacity_bitmap_realworld_offset_unit_in = opacity_bitmap_realworld_offset_unit_in1;// Output connection
	// Graph input 
	//{
int opacity_bitmap_realworld_offset_unit_unit_from1 = 3//};
;
	nodeTmp_opacity_bitmap_realworld_offset_unit_unit_from = opacity_bitmap_realworld_offset_unit_unit_from1;// Output connection
	// Graph input 
	//{
int opacity_bitmap_realworld_offset_unit_unit_to//};
 = distance_unit;
	nodeTmp_opacity_bitmap_realworld_offset_unit_unit_to = opacity_bitmap_realworld_offset_unit_unit_to;// Output connection
	// Graph input function call realworld_offset (See definition IM_distance_unit_vector2_genglsl)
{
	//{
    vec2 opacity_bitmap_realworld_offset_unit_out = nodeTmp_opacity_bitmap_realworld_offset_unit_in * mx_distance_unit_ratio(nodeTmp_opacity_bitmap_realworld_offset_unit_unit_from, nodeTmp_opacity_bitmap_realworld_offset_unit_unit_to);
//};
	nodeOutTmp_opacity_bitmap_realworld_offset_unit_out = opacity_bitmap_realworld_offset_unit_out;// Output connection
	nodeTmp_opacity_bitmap_realworld_offset = opacity_bitmap_realworld_offset_unit_out;// Output connection
}
	//Temp input variables for opacity_bitmap_realworld_scale_unit 
	vec2 nodeOutTmp_opacity_bitmap_realworld_scale_unit_out; //Temp output variable for out 
	vec2 nodeTmp_opacity_bitmap_realworld_scale_unit_in; //Temp input variable for in 
	int nodeTmp_opacity_bitmap_realworld_scale_unit_unit_from; //Temp input variable for unit_from 
	int nodeTmp_opacity_bitmap_realworld_scale_unit_unit_to; //Temp input variable for unit_to 
	// Graph input TestMaskWithChromeKeyDecal_nodegraph/opacity_bitmap/realworld_scale
	//{
vec2 opacity_bitmap_realworld_scale_unit_in1//};
 = material.opacity_bitmap_realworld_scale;
	nodeTmp_opacity_bitmap_realworld_scale_unit_in = opacity_bitmap_realworld_scale_unit_in1;// Output connection
	// Graph input 
	//{
int opacity_bitmap_realworld_scale_unit_unit_from1 = 3//};
;
	nodeTmp_opacity_bitmap_realworld_scale_unit_unit_from = opacity_bitmap_realworld_scale_unit_unit_from1;// Output connection
	// Graph input 
	//{
int opacity_bitmap_realworld_scale_unit_unit_to//};
 = distance_unit;
	nodeTmp_opacity_bitmap_realworld_scale_unit_unit_to = opacity_bitmap_realworld_scale_unit_unit_to;// Output connection
	// Graph input function call realworld_scale (See definition IM_distance_unit_vector2_genglsl)
{
	//{
    vec2 opacity_bitmap_realworld_scale_unit_out = nodeTmp_opacity_bitmap_realworld_scale_unit_in * mx_distance_unit_ratio(nodeTmp_opacity_bitmap_realworld_scale_unit_unit_from, nodeTmp_opacity_bitmap_realworld_scale_unit_unit_to);
//};
	nodeOutTmp_opacity_bitmap_realworld_scale_unit_out = opacity_bitmap_realworld_scale_unit_out;// Output connection
	nodeTmp_opacity_bitmap_realworld_scale = opacity_bitmap_realworld_scale_unit_out;// Output connection
}
	//Temp input variables for opacity_bitmap_uv_offset_unit 
	vec2 nodeOutTmp_opacity_bitmap_uv_offset_unit_out; //Temp output variable for out 
	vec2 nodeTmp_opacity_bitmap_uv_offset_unit_in; //Temp input variable for in 
	int nodeTmp_opacity_bitmap_uv_offset_unit_unit_from; //Temp input variable for unit_from 
	int nodeTmp_opacity_bitmap_uv_offset_unit_unit_to; //Temp input variable for unit_to 
	// Graph input TestMaskWithChromeKeyDecal_nodegraph/opacity_bitmap/uv_offset
	//{
vec2 opacity_bitmap_uv_offset_unit_in1//};
 = material.opacity_bitmap_uv_offset;
	nodeTmp_opacity_bitmap_uv_offset_unit_in = opacity_bitmap_uv_offset_unit_in1;// Output connection
	// Graph input 
	//{
int opacity_bitmap_uv_offset_unit_unit_from1 = 3//};
;
	nodeTmp_opacity_bitmap_uv_offset_unit_unit_from = opacity_bitmap_uv_offset_unit_unit_from1;// Output connection
	// Graph input 
	//{
int opacity_bitmap_uv_offset_unit_unit_to//};
 = distance_unit;
	nodeTmp_opacity_bitmap_uv_offset_unit_unit_to = opacity_bitmap_uv_offset_unit_unit_to;// Output connection
	// Graph input function call uv_offset (See definition IM_distance_unit_vector2_genglsl)
{
	//{
    vec2 opacity_bitmap_uv_offset_unit_out = nodeTmp_opacity_bitmap_uv_offset_unit_in * mx_distance_unit_ratio(nodeTmp_opacity_bitmap_uv_offset_unit_unit_from, nodeTmp_opacity_bitmap_uv_offset_unit_unit_to);
//};
	nodeOutTmp_opacity_bitmap_uv_offset_unit_out = opacity_bitmap_uv_offset_unit_out;// Output connection
	nodeTmp_opacity_bitmap_uv_offset = opacity_bitmap_uv_offset_unit_out;// Output connection
}
	//Temp input variables for opacity_bitmap_uv_scale_unit 
	vec2 nodeOutTmp_opacity_bitmap_uv_scale_unit_out; //Temp output variable for out 
	vec2 nodeTmp_opacity_bitmap_uv_scale_unit_in; //Temp input variable for in 
	int nodeTmp_opacity_bitmap_uv_scale_unit_unit_from; //Temp input variable for unit_from 
	int nodeTmp_opacity_bitmap_uv_scale_unit_unit_to; //Temp input variable for unit_to 
	// Graph input TestMaskWithChromeKeyDecal_nodegraph/opacity_bitmap/uv_scale
	//{
vec2 opacity_bitmap_uv_scale_unit_in1//};
 = material.opacity_bitmap_uv_scale;
	nodeTmp_opacity_bitmap_uv_scale_unit_in = opacity_bitmap_uv_scale_unit_in1;// Output connection
	// Graph input 
	//{
int opacity_bitmap_uv_scale_unit_unit_from1 = 3//};
;
	nodeTmp_opacity_bitmap_uv_scale_unit_unit_from = opacity_bitmap_uv_scale_unit_unit_from1;// Output connection
	// Graph input 
	//{
int opacity_bitmap_uv_scale_unit_unit_to//};
 = distance_unit;
	nodeTmp_opacity_bitmap_uv_scale_unit_unit_to = opacity_bitmap_uv_scale_unit_unit_to;// Output connection
	// Graph input function call uv_scale (See definition IM_distance_unit_vector2_genglsl)
{
	//{
    vec2 opacity_bitmap_uv_scale_unit_out = nodeTmp_opacity_bitmap_uv_scale_unit_in * mx_distance_unit_ratio(nodeTmp_opacity_bitmap_uv_scale_unit_unit_from, nodeTmp_opacity_bitmap_uv_scale_unit_unit_to);
//};
	nodeOutTmp_opacity_bitmap_uv_scale_unit_out = opacity_bitmap_uv_scale_unit_out;// Output connection
	nodeTmp_opacity_bitmap_uv_scale = opacity_bitmap_uv_scale_unit_out;// Output connection
}
	// Graph input TestMaskWithChromeKeyDecal_nodegraph/opacity_bitmap/rotation_angle
	//{
float opacity_bitmap_rotation_angle1//};
 = material.opacity_bitmap_rotation_angle;
	nodeTmp_opacity_bitmap_rotation_angle = opacity_bitmap_rotation_angle1;// Output connection
	// Graph input TestMaskWithChromeKeyDecal_nodegraph/opacity_bitmap/rgbamount
	//{
float opacity_bitmap_rgbamount1 = 1//};
;
	nodeTmp_opacity_bitmap_rgbamount = opacity_bitmap_rgbamount1;// Output connection
	// Graph input TestMaskWithChromeKeyDecal_nodegraph/opacity_bitmap/invert
	//{
bool opacity_bitmap_invert1 = false//};
;
	nodeTmp_opacity_bitmap_invert = opacity_bitmap_invert1;// Output connection
	// Graph input TestMaskWithChromeKeyDecal_nodegraph/opacity_bitmap/uaddressmode
	//{
int opacity_bitmap_uaddressmode1 = 1//};
;
	nodeTmp_opacity_bitmap_uaddressmode = opacity_bitmap_uaddressmode1;// Output connection
	// Graph input TestMaskWithChromeKeyDecal_nodegraph/opacity_bitmap/vaddressmode
	//{
int opacity_bitmap_vaddressmode1 = 1//};
;
	nodeTmp_opacity_bitmap_vaddressmode = opacity_bitmap_vaddressmode1;// Output connection
	// Graph input TestMaskWithChromeKeyDecal_nodegraph/opacity_bitmap/uv_index
	//{
int opacity_bitmap_uv_index1 = 0//};
;
	nodeTmp_opacity_bitmap_uv_index = opacity_bitmap_uv_index1;// Output connection
	// Graph input function call fg (See definition adsk:NG_adsk_bitmap_color3)
{
	//{
    vec3 opacity_bitmap_out = vec3(0.0);
    adsk_NG_adsk_bitmap_color3(nodeTmp_opacity_bitmap_file, nodeTmp_opacity_bitmap_realworld_offset, nodeTmp_opacity_bitmap_realworld_scale, nodeTmp_opacity_bitmap_uv_offset, nodeTmp_opacity_bitmap_uv_scale, nodeTmp_opacity_bitmap_rotation_angle, nodeTmp_opacity_bitmap_rgbamount, nodeTmp_opacity_bitmap_invert, nodeTmp_opacity_bitmap_uaddressmode, nodeTmp_opacity_bitmap_vaddressmode, nodeTmp_opacity_bitmap_uv_index, opacity_bitmap_out);
//};
	nodeOutTmp_opacity_bitmap_out = opacity_bitmap_out;// Output connection
	nodeTmp_difference_image_with_chromekey_fg = opacity_bitmap_out;// Output connection
}
	// Graph input TestMaskWithChromeKeyDecal_nodegraph/difference_image_with_chromekey/bg
	//{
vec3 difference_image_with_chromekey_bg1//};
 = material.difference_image_with_chromekey_bg;
	nodeTmp_difference_image_with_chromekey_bg = difference_image_with_chromekey_bg1;// Output connection
	// Graph input TestMaskWithChromeKeyDecal_nodegraph/difference_image_with_chromekey/mix
	//{
float difference_image_with_chromekey_mix1//};
 = material.difference_image_with_chromekey_mix;
	nodeTmp_difference_image_with_chromekey_mix = difference_image_with_chromekey_mix1;// Output connection
	// Graph input function call in (See definition IM_difference_color3_genglsl)
{
	//{
    vec3 difference_image_with_chromekey_out = (nodeTmp_difference_image_with_chromekey_mix*abs(nodeTmp_difference_image_with_chromekey_bg - nodeTmp_difference_image_with_chromekey_fg)) + ((1.0-nodeTmp_difference_image_with_chromekey_mix)*nodeTmp_difference_image_with_chromekey_bg);
//};
	nodeOutTmp_difference_image_with_chromekey_out = difference_image_with_chromekey_out;// Output connection
	nodeTmp_convert_color3_to_vector3_in = difference_image_with_chromekey_out;// Output connection
}
	// Graph input function call in (See definition IM_convert_color3_vector3_genglsl)
{
	//{
    vec3 convert_color3_to_vector3_out = vec3(nodeTmp_convert_color3_to_vector3_in.x, nodeTmp_convert_color3_to_vector3_in.y, nodeTmp_convert_color3_to_vector3_in.z);
//};
	nodeOutTmp_convert_color3_to_vector3_out = convert_color3_to_vector3_out;// Output connection
	nodeTmp_magnitude_vector3_in = convert_color3_to_vector3_out;// Output connection
}
	// Graph input function call value1 (See definition IM_magnitude_vector3_genglsl)
{
	//{
    float magnitude_vector3_out = length(nodeTmp_magnitude_vector3_in);
//};
	nodeOutTmp_magnitude_vector3_out = magnitude_vector3_out;// Output connection
	nodeTmp_convert_to_opacity_white_black_value1 = magnitude_vector3_out;// Output connection
}
	// Graph input TestMaskWithChromeKeyDecal_nodegraph/convert_to_opacity_white_black/value2
	//{
float convert_to_opacity_white_black_value21//};
 = material.convert_to_opacity_white_black_value2;
	nodeTmp_convert_to_opacity_white_black_value2 = convert_to_opacity_white_black_value21;// Output connection
	// Graph input TestMaskWithChromeKeyDecal_nodegraph/convert_to_opacity_white_black/in1
	//{
vec3 convert_to_opacity_white_black_in11//};
 = material.convert_to_opacity_white_black_in1;
	nodeTmp_convert_to_opacity_white_black_in1 = convert_to_opacity_white_black_in11;// Output connection
	// Graph input TestMaskWithChromeKeyDecal_nodegraph/convert_to_opacity_white_black/in2
	//{
vec3 convert_to_opacity_white_black_in21//};
 = material.convert_to_opacity_white_black_in2;
	nodeTmp_convert_to_opacity_white_black_in2 = convert_to_opacity_white_black_in21;// Output connection
	// Graph input function call opacity (See definition IM_ifgreater_color3_genglsl)
{
	//{
    vec3 convert_to_opacity_white_black_out = (nodeTmp_convert_to_opacity_white_black_value1 > nodeTmp_convert_to_opacity_white_black_value2) ? nodeTmp_convert_to_opacity_white_black_in1 : nodeTmp_convert_to_opacity_white_black_in2;
//};
	nodeOutTmp_convert_to_opacity_white_black_out = convert_to_opacity_white_black_out;// Output connection
	opacity = convert_to_opacity_white_black_out;// Output connection
}
	// Graph input TestMaskWithChromeKeyDecal/thin_walled
	//{
bool TestMaskWithChromeKeyDecal_thin_walled//};
 = material.thin_walled;
	thin_walled = TestMaskWithChromeKeyDecal_thin_walled;// Output connection
}
