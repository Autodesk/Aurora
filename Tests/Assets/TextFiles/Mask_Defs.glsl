
// Definition for implementation IM_texcoord_vector2_genglsl
//{
//};

// Definition for implementation IM_image_color3_genglsl
//{
    // Included from lib/$fileTransformUv
    vec2 mx_transform_uv(vec2 uv, vec2 uv_scale, vec2 uv_offset)
    {
        uv = uv * uv_scale + uv_offset;
        return uv;
    }
    
    
    void mx_image_color3(sampler2D tex_sampler, int layer, vec3 defaultval, vec2 texcoord, int uaddressmode, int vaddressmode, int filtertype, int framerange, int frameoffset, int frameendaction, vec2 uv_scale, vec2 uv_offset, out vec3 result)
    {
        if (textureSize(tex_sampler, 0).x > 1)
        {
            vec2 uv = mx_transform_uv(texcoord, uv_scale, uv_offset);
            result = texture(tex_sampler, uv).rgb;
        }
        else
        {
            result = defaultval;
        }
    }

//};

// Definition for implementation IM_distance_unit_vector2_genglsl
//{
    float mx_distance_unit_ratio(int unit_from, int unit_to)
    {
        const float u_distance_unit_scales[10] = float[10](0.000000001000000, 0.000000999999997, 0.001000000047497, 0.009999999776483, 0.025399999693036, 0.304800003767014, 0.914399981498718, 1.000000000000000, 1000.000000000000000, 1609.343994140625000);
        return (u_distance_unit_scales[unit_from] / u_distance_unit_scales[unit_to]);
    }

//};

// Definition for implementation adsk:NG_adsk_bitmap_color3
//{
    void mx_rotate_vector2(vec2 _in, float amount, out vec2 result)
    {
        float rotationRadians = radians(amount);
        float sa = sin(rotationRadians);
        float ca = cos(rotationRadians);
        result = vec2(ca*_in.x + sa*_in.y, -sa*_in.x + ca*_in.y);
    }

    void NG_place2d_vector2(vec2 texcoord1, vec2 pivot, vec2 scale, float rotate, vec2 offset, int operationorder, out vec2 out1)
    {
        vec2 N_subpivot_out = texcoord1 - pivot;
        vec2 N_applyscale_out = N_subpivot_out / scale;
        vec2 N_applyoffset2_out = N_subpivot_out - offset;
        vec2 N_applyrot_out = vec2(0.0);
        mx_rotate_vector2(N_applyscale_out, rotate, N_applyrot_out);
        vec2 N_applyrot2_out = vec2(0.0);
        mx_rotate_vector2(N_applyoffset2_out, rotate, N_applyrot2_out);
        vec2 N_applyoffset_out = N_applyrot_out - offset;
        vec2 N_applyscale2_out = N_applyrot2_out / scale;
        vec2 N_addpivot_out = N_applyoffset_out + pivot;
        vec2 N_addpivot2_out = N_applyscale2_out + pivot;
        vec2 N_switch_operationorder_out = vec2(0.0);
        if (float(operationorder) < float(1))
        {
            N_switch_operationorder_out = N_addpivot_out;
        }
        else if (float(operationorder) < float(2))
        {
            N_switch_operationorder_out = N_addpivot2_out;
        }
        else if (float(operationorder) < float(3))
        {
            N_switch_operationorder_out = vec2(0, 0);
        }
        else if (float(operationorder) < float(4))
        {
            N_switch_operationorder_out = vec2(0, 0);
        }
        else if (float(operationorder) < float(5))
        {
            N_switch_operationorder_out = vec2(0, 0);
        }
        out1 = N_switch_operationorder_out;
    }

    void adsk_NG_adsk_bitmap_color3(sampler2D file, vec2 realworld_offset, vec2 realworld_scale, vec2 uv_offset, vec2 uv_scale, float rotation_angle, float rgbamount, bool invert, int uaddressmode, int vaddressmode, int uv_index, out vec3 out1)
    {
        vec2 texcoord1_out = vertexData.texCoord;
        vec2 total_offset_out = realworld_offset + uv_offset;
        vec2 total_scale_out = realworld_scale / uv_scale;
        const float rotation_angle_param_in2_tmp = -1;
        float rotation_angle_param_out = rotation_angle * rotation_angle_param_in2_tmp;
        vec2 a_place2d_out = vec2(0.0);
        NG_place2d_vector2(texcoord1_out, vec2(0, 0), total_scale_out, rotation_angle_param_out, total_offset_out, 0, a_place2d_out);
        vec3 b_image_out = vec3(0.0);
        mx_image_color3(file, 0, vec3(0, 0, 0), a_place2d_out, uaddressmode, vaddressmode, 1, 0, 0, 0, vec2(1, 1), vec2(0, 0), b_image_out);
        vec3 image_brightness_out = b_image_out * rgbamount;
        const vec3 image_invert_amount_tmp = vec3(1, 1, 1);
        vec3 image_invert_out = image_invert_amount_tmp - image_brightness_out;
        const bool image_convert_value2_tmp = true;
        vec3 image_convert_out = (invert == image_convert_value2_tmp) ? image_invert_out : image_brightness_out;
        out1 = image_convert_out;
    }

//};

// Definition for implementation IM_difference_color3_genglsl
//{
//};

// Definition for implementation IM_convert_color3_vector3_genglsl
//{
//};

// Definition for implementation IM_magnitude_vector3_genglsl
//{
//};

// Definition for implementation IM_ifgreater_color3_genglsl
//{
//};
