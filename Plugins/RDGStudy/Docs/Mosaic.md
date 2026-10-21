# Mosaic

같은 모자이크를 **RDG 패스**와 **포스트 프로세스 머티리얼**로 구현해 비용을 비교하기 위한 학습용 구현.

## 파일
| 파일 | 역할 |
|---|---|
| `Shaders/Mosaic.ush` | 블록 인덱스/범위/중심 계산 (순수 HLSL, 공용) |
| `Shaders/Mosaic.usf` | RDG 글로벌 쉐이더 (A/B PS, C 다운샘플 PS, D 리덕션 CS, 업스케일 PS) |
| `Shaders/MosaicMaterial.ush` | 머티리얼 Custom 노드용 래퍼 (`SceneTextureLookup` 사용) |
| `Source/.../MosaicShaders.h/.cpp` | 글로벌 쉐이더 클래스, 파라미터 구조체 |
| `Source/.../MosaicViewExtension.h/.cpp` | Tonemap 이후 패스 등록, CVar |

## RDG 버전 (콘솔)
| CVar | 기본값 | 설명 |
|---|---|---|
| `r.Mosaic.Enable` | 0 | 1 이면 익스텐션 활성. 0 이면 `IsActiveThisFrame` 에서 빠져 비용 0 |
| `r.Mosaic.Method` | 0 | 0 포인트 / 1 N*N 브루트포스 / 2 다운샘플+업스케일 / 3 컴퓨트 리덕션+업스케일 |
| `r.Mosaic.BlockSize` | 16 | 2~128, 짝수로 올림 |
| `r.Mosaic.UseStencil` | 0 | 1 이면 CustomStencil == StencilValue 픽셀만 |
| `r.Mosaic.StencilValue` | 1 | 0~255 |
| `r.RDGStudy.Triangle` | 0 | 기존 삼각형 테스트 패스 |

스텐실을 쓰려면 `r.CustomDepth=3`(DefaultEngine.ini 에 설정됨)이 필요하고, 대상 메시의 **Render CustomDepth Pass = On**, **CustomDepth Stencil Value = 1** 로 지정한다.

## 머티리얼 버전 (에디터에서 제작)
`Plugins/RDGStudy/Content/Mosaic/M_PP_Mosaic`
1. Material Domain = **Post Process**, Blendable Location = **After Tonemapping** (RDG 패스와 같은 위치)
2. Custom 노드 `Mosaic`
   - Include File Paths: `/CustomShaders/MosaicMaterial.ush`
   - Output Type: Float4 (CMOT Float 4)
   - Inputs: `ViewportUV` ← ScreenPosition 노드의 ViewportUV, `BlockSize` ← Scalar Param, `Method` ← Scalar Param (0/1)
   - Code: `return MosaicMaterial(ViewportUV, BlockSize, (int)Method);`
3. (스텐실) Custom 노드 `MosaicMask`
   - Include File Paths: `/CustomShaders/MosaicMaterial.ush`, Output Type: Float1
   - Inputs: `ViewportUV`, `StencilValue` ← Scalar Param
   - Code: `return MosaicMaterialStencilMask(ViewportUV, StencilValue);`
   - `SceneTexture:PostProcessInput0`, `Mosaic`, Mask 를 Lerp(A=원본, B=모자이크, Alpha=Mask×UseStencil 스위치) 로 연결
   - **주의**: 머티리얼 그래프 어딘가에서 `SceneTexture:CustomStencil` 노드를 한 번 참조해야 CustomStencil 이 바인딩된다 (출력에 0 을 곱해 더하면 된다).
4. Emissive Color ← 결과 RGB
5. `MI_PP_Mosaic` 인스턴스를 만들어 PostProcessVolume(Infinite Extent/Unbound) → Rendering Features → Post Process Materials 에 추가

> Custom 노드의 `Method` 는 런타임 분기다. 측정용으로는 Static Switch 로 Custom 노드 두 개(포인트/평균)를 갈라두는 쪽이 RDG 퍼뮤테이션과 같은 조건이 된다.

## 측정
- 공통: `r.ScreenPercentage 100`, 해상도 고정(`r.SetRes 1920x1080w`), `r.VSync 0`
- `stat gpu` → `Mosaic` 항목 / 머티리얼은 `PostProcessMaterial`
- `ProfileGPU` → `Mosaic <Method> Block=<N>` 이벤트 아래 개별 패스 시간
- RenderDoc: `Mosaic.Downsample`, `Mosaic.Reduce`, `Mosaic.Upsample` 이벤트와 `Mosaic.LowRes` 텍스처 확인
- 비교 매트릭스: Method 0~3 × BlockSize 8/16/32/64 × 1080p/4K, 스텐실 on/off

### 예상 경향 (측정으로 확인할 것)
- **0 포인트**: 픽셀당 1 Load. 가장 싸지만 카메라 이동 시 깜빡임
- **1 브루트포스**: 픽셀당 N² Load → BlockSize 에 제곱으로 비례
- **2 다운샘플**: 블록당 N²/4 bilinear tap, 전체 ≈ 화면 픽셀 수 / 4 fetch + 업스케일 1 Load
- **3 컴퓨트**: 전체 ≈ 화면 픽셀 수 Load. 블록이 작으면(< 8) 64 스레드 중 다수가 놀아서 비효율 → 2 와의 교차점을 찾아볼 것
- 스텐실: 셰이더 분기라 마스크 밖도 셰이더는 실행된다. 다음 단계로 하드웨어 스텐실 테스트(DepthStencilState) 컬링을 시도해볼 수 있다
