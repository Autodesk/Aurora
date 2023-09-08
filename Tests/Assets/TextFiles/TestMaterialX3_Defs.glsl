
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
