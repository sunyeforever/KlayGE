float4 PointSpriteVertexExtend(float4 pos, float2 tex, float point_size,
			float4x4 view, float4x4 proj,
			float2 viewport_width_height,
			float3 distance_attenuation)
{
	float4 ret;

	float4 view_pos = mul(pos, view);
	ret = mul(view_pos, proj);
	float d = length(view_pos.xyz);
	float derived_size = point_size
		* saturate(sqrt(1.0f / (distance_attenuation.x + distance_attenuation.y * d + distance_attenuation.z * d * d)));
	float2 xy_size = derived_size / viewport_width_height;

	ret.xy += xy_size * (tex * 2 - 1);
	ret.w = 1;

	return ret;
}
