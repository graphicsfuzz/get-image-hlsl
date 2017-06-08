cbuffer InjectionSwitch : register( b0 ) {
  float2 injectionSwitch;
};


struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float3 color : COLOR0;
};

float4 main(PixelShaderInput input) : SV_TARGET
{
  if(injectionSwitch.x < injectionSwitch.y){
    return float4(input.pos.x / 256, input.pos.y / 256, 1.0f, 1.0f);
  } else {
    return 0;
  }
}
