# Testes do Freya

## Executar os testes unitários

Requer o mesmo toolchain da biblioteca: CMake, Ninja, GCC 16+ e Vulkan SDK.
Os testes unitários não criam janela, dispositivo Vulkan ou recursos de GPU.

```sh
cmake -B build -S . -G Ninja -DFREYA_BUILD_TESTS=ON
cmake --build build --parallel --target FreyaTests FreyaSceneTests
ctest --test-dir build -L unit --output-on-failure
```

`FreyaSceneTests` fica separado porque `Scene.cpp` também contém o upload
para o renderer. Linkar esse objeto traz implementações que conflitam com
os substitutos de UI presentes em `FreyaTests`. Seus testes exercitam somente
o gerenciamento CPU de instâncias. O build padrão compila ambos os alvos,
e ambos são descobertos automaticamente pelo CTest com o rótulo `unit`.

## Áreas cobertas

| Área | Contratos principais |
| --- | --- |
| Core | Opções, limites, configuração de janela, sparse set e spinlock |
| Eventos | Identidade de tipos, publicação, inscrição e remoção de listeners |
| Renderização CPU | Layouts CPU/GPU, luzes, LOD, irradiância SH, escalas de UI e registros de técnicas |
| Sincronização | Upload staging, slots de frame, exclusão de operações e snapshots concorrentes |
| UI | Layout, recorte, widgets, foco, entrada, estilos e filas de desenho |
| Partículas e billboards | Emissão, capacidade, concorrência, strips, ribbons e splines |
| Poses | TRS, rest pose, interpolação, looping, máscaras, blends aditivos, hierarquia e skinning |
| Clips e bakes | Eventos e fronteiras de loop, origem de eventos, footsteps, layout frame-major, interpolação e fallback |
| Blend spaces | Resolução 1D/2D, pesos baricêntricos, degeneração, tempos e fase sincronizada |
| AnimGraph | Validação do builder, parâmetros, condições, triggers, crossfade, camadas, packing GPU e debug snapshot |
| Rig | Look-at, IK de dois ossos, preservação de comprimentos, root motion e locomoção planar |
| Cena | Instâncias, remoção idempotente, reutilização de slots, iteração, transformações e limpeza |
| Handles | Validade do ID zero, igualdade, ordenação, hashing e isolamento entre tipos de assets |
| Debug draw | Capacidade sem linhas parciais, círculos, esferas e esqueleto em espaço global |

Os testes adicionados em `unit/Scene/` e `unit/Rendering/` usam dados
sintéticos e expectativas numéricas independentes. Os helpers em
`unit/Support/AnimationFixtures.hpp` fornecem um esqueleto pequeno, clips
simples e comparações com tolerância; não carregam modelos externos.

## Limites do escopo

Esta suíte não comprova cobertura de 100% de linhas ou branches. Uploads
reais, shaders, passes de renderização, swapchains e resultados visuais
dependem de testes de integração com GPU, não apenas de testes unitários.
As transformações de round-trip testadas são TRS com escala positiva;
shear e escalas refletidas não estão cobertos por esses casos.

Os testes de integração são opcionais, requerem um Vulkan ICD e ficam em
`integration/`:

```sh
cmake -B build -S . -G Ninja -DFREYA_BUILD_GPU_TESTS=ON
cmake --build build --parallel --target FreyaGpuTests
ctest --test-dir build -L integration --output-on-failure
```

Ao criar novos testes, mantenha verificações de caminho normal, fronteiras,
entradas vazias/inválidas e invariantes relevantes. Não substitua resultados
esperados por chamadas à mesma função que está sendo testada.
