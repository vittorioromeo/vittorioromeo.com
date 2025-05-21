I recently released [**BubbleByte**](https://store.steampowered.com/app/3499760/BubbleByte/) on Steam, my second commercial game built with my [fork of SFML](https://vittorioromeo.com/index/blog/vrsfml.html).

It's an incremental/clicker/idle game where -- eventually -- the player will see thousands upon thousands of  particles on screen simultaneously.

Even with a basic AoS *(Array of Structures)* layout, the game's performance is great thanks to the draw batching system. However, I began wondering how much performance I might be leaving on the table by not adopting a SoA *(Structure of Arrays)* layout. Let's figure that out in this article!



### try the benchmark!

The benchmark simulates a large number of 2D particles that continuously change position, scale, opacity, and rotation. Through an ImGui-based UI[^imgui_in_vrsfml], you can choose the number of particles, toggle multithreading, and switch between AoS and SoA on the fly.

[^imgui_in_vrsfml]: My fork of SFML supports ImGui out of the box!

A demo is worth a thousand words, and since my [fork of SFML](https://github.com/vittorioromeo/VRSFML) supports Emscripten, you can try the benchmark directly in your browser. Play around with all the options -- I'm curious to hear what results you get!

- **[`particles.html` on `vittorioromeo/VRSFML_HTML5_Examples`](https://vittorioromeo.github.io/VRSFML_HTML5_Examples/particles.html)**

Note that the drawing step is not optimized at all -- each particle is turned into a `sf::Sprite` instance on the fly. This approach is only viable thanks to batching.

The source code for the benchmark is available [here](https://github.com/vittorioromeo/VRSFML/blob/bubble_idle/examples/particles/Particles.cpp).



### particle layout

In the AoS *(Array of Structures)* approach, each particle is encapsulated in a single structure:

```cpp
struct ParticleAoS
{
    sf::Vector2f position, velocity, acceleration;
    float scale, scaleGrowth;
    float opacity, opacityGrowth;
    float rotation, torque;
};

std::vector<ParticleAoS> particlesAoS;
```

Every particle’s complete set of properties is stored contiguously. While this layout is intuitive, it can be less cache-friendly when processing specific properties across all particles.

In contrast, the SoA *(Structure of Arrays)* layout stores each property into its own contiguous array. Using a custom template (`SoAFor`)[^soafor], the particle data is organized as follows:

[^soafor]: The `SoAFor<Ts...>` utility is a wrapper over a collection of `std::vector<Ts>...` that I wrote which provides a (somewhat) nice API to iterate over arbitrary subsets of "fields" by specifying their indices as template parameters. Code [here](https://github.com/vittorioromeo/VRSFML/blob/bubble_idle/examples/bubble_idle/SoA.hpp).

```cpp
using ParticleSoA = SoAFor<sf::Vector2f, // position
                           sf::Vector2f, // velocity
                           sf::Vector2f, // acceleration

                           float, // scale
                           float, // scaleGrowth

                           float, // opacity
                           float, // opacityGrowth

                           float,  // rotation
                           float>; // torque

ParticleSoA particlesSoA;
```

This columnar layout ensures that, when updating a specific field (e.g., adding acceleration to velocity), the memory accesses are more sequential and cache-friendly. The performance benefits become particularly evident when processing millions of particles in tight loops.



### particle update

Every frame, the system processes each particle -- applying acceleration, updating velocity and position, modifying scale and opacity, and adjusting rotation. The update method varies significantly between three approaches: "AoS", "SoA", and "SoA Unified".

In the AoS approach, the update loop simply iterates through a contiguous vector of `ParticleAoS` objects, modifying each field:

```cpp
for (ParticleAoS& p : particlesAoS)
{
    p.velocity += p.acceleration;
    p.position += p.velocity;
    p.scale += p.scaleGrowth;
    p.opacity += p.opacityGrowth;
    p.rotation += p.torque;
}
```

While straightforward, this approach may suffer from scattered memory accesses since it loads all properties for each particle even if only a subset is being updated at a time.

With SoA, each property is stored in its own contiguous array. The system updates one field across all particles before moving on to the next:

```cpp
particlesSoA.with<1, 2>(
    [](sf::Vector2f& vel, const sf::Vector2f& acc) { vel += acc; });

particlesSoA.with<0, 1>(
    [](sf::Vector2f& pos, sf::Vector2f& vel) { pos += vel; });

particlesSoA.with<3, 4>(
    [](float& scale, const float growth) { scale += growth; });

particlesSoA.with<5, 6>(
    [](float& opacity, const float growth) { opacity += growth; });

particlesSoA.with<7, 8>(
    [](float& rotation, const float torque) { rotation += torque; });
```

This method minimizes cache misses and opens up opportunities for SIMD optimizations. However, it still requires multiple passes over the data.

The "SoA Unified" approach fuses all updates into a single loop:

```cpp
particlesSoA.withAll(
    [](sf::Vector2f& pos, sf::Vector2f& vel, const sf::Vector2f acc,
       float& scale, const float scaleGrowth,
       float& opacity, const float opacityGrowth,
       float& rotation, const float torque)
{
    vel += acc;
    pos += vel;
    scale += scaleGrowth;
    opacity += opacityGrowth;
    rotation += torque;
});
```

By reducing the iteration count, this approach minimizes loop overhead. However, accessing multiple attributes of a single particle in one pass may limit memory prefetching benefits and could inhibit SIMD optimizations.



### repopulation

As particles fade (i.e., when opacity falls below a threshold), they are removed and new particles are spawned to maintain a constant count. The repopulation is handled by resizing the vectors every frame (as needed):

```cpp
const auto populateParticlesAoS = [&](const std::size_t n)
{
    if (n < particlesAoS.size())
    {
        particlesAoS.resize(n);
        return;
    }

    particlesAoS.reserve(n);
    for (std::size_t i = particlesAoS.size(); i < n; ++i)
        pushParticle([&](auto&&... xs) { particlesAoS.emplace_back(xs...); });
};

// ...equivalent version for SoA...
```



### multithreading

With millions of particles in play, a single-threaded update loop can become a bottleneck. To address this, the simulation leverages a thread pool[^thread_pool] to parallelize the update work. A helper lambda distributes particle processing across available CPU cores:

[^thread_pool]: I made it myself. It's pretty basic, but gets the job done. Code [here](https://github.com/vittorioromeo/VRSFML/blob/bubble_idle/include/SFML/Base/ThreadPool.hpp).

```cpp
const auto doInBatches = [&](const std::size_t totalParticles, auto&& task)
{
    const std::size_t particlesPerBatch = totalParticles / nWorkers;
    std::latch latch(static_cast<std::ptrdiff_t>(nWorkers));

    for (std::size_t i = 0; i < nWorkers; ++i)
    {
        pool.post([&, i]
        {
            const std::size_t batchStart = i * particlesPerBatch;

            const std::size_t batchEnd =
                (i == nWorkers - 1) ? totalParticles
                                    : (i + 1) * particlesPerBatch;

            task(i, batchStart, batchEnd);
            latch.count_down();
        });
    }

    latch.wait();
};
```

This lambda divides the particle array into non-overlapping chunks that are processed concurrently. When multithreading is enabled, the repopulation step becomes the bottleneck -- I’m sure there’s a clever way to parallelize that step too (for example, by processing in chunks and compressing at the end), but that’s an exercise for the reader :)



### benchmark results

*(Hardware used: i9 13900k, RTX 4090.)*

The results confirm that SoA consistently outperforms AoS, especially as the number of particles increases. The "Unified" SoA update method yields mixed results—sometimes providing further gains by reducing iteration overhead, though not always enough to be included in later benchmarks.

Incorporating a repopulation routine adds extra overhead because particles that reach zero opacity are removed and new ones are spawned. This extra work increases update times in both single-threaded and multi-threaded modes. Even so, when drawing is included -- *"Multi-Threaded + Repopulation + Draw"* -- the benefit of using SoA over AoS remains significant, despite the additional bottleneck from rendering calls.

<script src=https://cdnjs.cloudflare.com/ajax/libs/Chart.js/3.9.1/chart.min.js></script><style>.chart-container{width:90%;max-width:800px;height:500px;margin:20px auto}.description{width:90%;max-width:800px;margin:10px auto;font-size:14px;color:#666}.tabs{justify-content:center;margin-bottom:20px;width:90%;max-width:800px}.tab{max-width:400px;padding:10px 20px;background-color:#f0f0f0;border:1px solid #ccc;cursor:pointer;transition:background-color .3s;font-weight:500}.tab:first-child{border-radius:4px 0 0 4px}.tab:last-child{border-radius:0 4px 4px 0}.tab.active{background-color:#007bff;color:#fff;border-color:#007bff}.tab:hover:not(.active){background-color:#e0e0e0}</style><div class=tabs><div class="tab active"onclick='switchDataset("singleThreaded")'>Single-Threaded</div><div class=tab onclick='switchDataset("multiThreaded")'>Multi-Threaded</div><div class=tab onclick='switchDataset("singleThreadedRepopulation")'>Single-Threaded + Repopulation</div><div class=tab onclick='switchDataset("multiThreadedRepopulation")'>Multi-Threaded + Repopulation</div><div class=tab onclick='switchDataset("multiThreadedRepopulationDraw")'>Multi-Threaded + Repopulation + Draw</div></div><div class=chart-container><canvas id=benchmarkChart></canvas></div><div class=description><p>Lower update time (ms) is better. Higher FPS is better.<p></div><script>// Single-threaded benchmark data (unified processing)
    const singleThreadedData = [
      {
        numEntities: 500000,
        noSoA: 0.696782,
        noSoAFPS: 1206.677856,
        SoA: 0.511396,
        SoAFPS: 1538.295532,
        SoAUnified: 0.659552,
        SoAUnifiedFPS: 1219.666016
      },
      {
        numEntities: 1000000,
        noSoA: 2.431151,
        noSoAFPS: 381.553223,
        SoA: 1.594551,
        SoAFPS: 551.403931,
        SoAUnified: 1.335823,
        SoAUnifiedFPS: 625.798584
      },
      {
        numEntities: 2000000,
        noSoA: 5.295929,
        noSoAFPS: 182.056137,
        SoA: 3.761077,
        SoAFPS: 241.100723,
        SoAUnified: 2.804883,
        SoAUnifiedFPS: 333.971680
      },
      {
        numEntities: 4000000,
        noSoA: 11.928971,
        noSoAFPS: 82.063019,
        SoA: 7.817771,
        SoAFPS: 123.061229,
        SoAUnified: 5.757132,
        SoAUnifiedFPS: 167.244135
      }
    ];

    // Multi-threaded benchmark data (unified processing)
    const multiThreadedData = [
      {
        numEntities: 500000,
        noSoA: 0.116159,
        noSoAFPS: 3255.920667,
        SoA: 0.091162,
        SoAFPS: 3844.829333,
        SoAUnified: 0.091097,
        SoAUnifiedFPS: 4282.866000
      },
      {
        numEntities: 1000000,
        noSoA: 0.264258,
        noSoAFPS: 2199.420667,
        SoA: 0.258597,
        SoAFPS: 2169.176000,
        SoAUnified: 0.241081,
        SoAUnifiedFPS: 2318.304167
      },
      {
        numEntities: 2000000,
        noSoA: 1.771829,
        noSoAFPS: 456.218792,
        SoA: 1.406226,
        SoAFPS: 548.895333,
        SoAUnified: 1.491801,
        SoAUnifiedFPS: 536.572625
      },
      {
        numEntities: 4000000,
        noSoA: 4.592842,
        noSoAFPS: 200.608792,
        SoA: 3.720358,
        SoAFPS: 240.560812,
        SoAUnified: 3.976511,
        SoAUnifiedFPS: 229.465917
      }
    ];

    // Single-threaded repopulation benchmark data (no unified processing)
    const singleThreadedRepopulationData = [
      {
        numEntities: 500000,
        noSoA: 1.315258,
        noSoAFPS: 679.760958,
        SoA: 0.757495,
        SoAFPS: 1075.384917
      },
      {
        numEntities: 1000000,
        noSoA: 4.275830,
        noSoAFPS: 218.145979,
        SoA: 2.049859,
        SoAFPS: 437.071208
      },
      {
        numEntities: 2000000,
        noSoA: 9.407948,
        noSoAFPS: 102.935031,
        SoA: 4.672316,
        SoAFPS: 199.963417
      },
      {
        numEntities: 4000000,
        noSoA: 19.469867,
        noSoAFPS: 50.450500,
        SoA: 9.417798,
        SoAFPS: 102.568229
      }
    ];

    // Multi-threaded repopulation benchmark data (no unified processing)
    const multiThreadedRepopulationData = [
      {
        numEntities: 500000,
        noSoA: 0.692378,
        noSoAFPS: 1126.484750,
        SoA: 0.260272,
        SoAFPS: 2449.870333
      },
      {
        numEntities: 1000000,
        noSoA: 2.710052,
        noSoAFPS: 331.241083,
        SoA: 0.604021,
        SoAFPS: 1212.976083
      },
      {
        numEntities: 2000000,
        noSoA: 5.817028,
        noSoAFPS: 157.710677,
        SoA: 2.134021,
        SoAFPS: 389.234750
      },
      {
        numEntities: 4000000,
        noSoA: 12.583215,
        noSoAFPS: 76.681562,
        SoA: 5.375057,
        SoAFPS: 171.695417
      }
    ];

    // Multi-threaded repopulation + draw benchmark data (no unified processing)
    const multiThreadedRepopulationDrawData = [
      {
        numEntities: 500000,
        noSoA: 1.435542,
        noSoAFPS: 150.011906,
        SoA: 0.596290,
        SoAFPS: 178.656354
      },
      {
        numEntities: 1000000,
        noSoA: 2.910997,
        noSoAFPS: 73.116453,
        SoA: 1.282604,
        SoAFPS: 85.630859
      },
      {
        numEntities: 2000000,
        noSoA: 6.234144,
        noSoAFPS: 35.987602,
        SoA: 2.710358,
        SoAFPS: 42.241039
      }
    ];

    // Set default dataset
    let currentData = singleThreadedData;
    let chart;

    // Function to switch between datasets
    function switchDataset(datasetType) {
      // Remove active styling from all tabs
      document.querySelectorAll('.tab').forEach(tab => {
        tab.classList.remove('active');
      });
      // Add active class to clicked tab
      event.target.classList.add('active');

      if (datasetType === 'singleThreaded') {
        currentData = singleThreadedData;
      } else if (datasetType === 'multiThreaded') {
        currentData = multiThreadedData;
      } else if (datasetType === 'singleThreadedRepopulation') {
        currentData = singleThreadedRepopulationData;
      } else if (datasetType === 'multiThreadedRepopulation') {
        currentData = multiThreadedRepopulationData;
      } else if (datasetType === 'multiThreadedRepopulationDraw') {
        currentData = multiThreadedRepopulationDrawData;
      }
      updateChartData();
    }

    // Format numbers with commas
    function formatNumber(num) {
      return num.toString().replace(/\B(?=(\d{3})+(?!\d))/g, ",");
    }

    // Extract chart data based on current dataset
    function getChartData() {
      // Check if the current data includes unified processing data
      const hasUnified = currentData.length > 0 && currentData[0].hasOwnProperty('SoAUnified');
      let chartData = {
        labels: currentData.map(item => item.numEntities),
        noSoAData: currentData.map(item => item.noSoA),
        SoAData: currentData.map(item => item.SoA),
        noSoAFPS: currentData.map(item => Math.round(item.noSoAFPS)),
        SoAFPS: currentData.map(item => Math.round(item.SoAFPS))
      };
      if (hasUnified) {
        chartData.SoAUnifiedData = currentData.map(item => item.SoAUnified);
        chartData.SoAUnifiedFPS = currentData.map(item => Math.round(item.SoAUnifiedFPS));
      }
      return chartData;
    }

    // Update chart data by destroying and recreating chart
    function updateChartData() {
      const chartData = getChartData();
      if (chart) {
        chart.destroy();
      }
      createChart(chartData);
    }

    // Create the chart with conditional datasets (2 or 3 series)
    function createChart(chartData) {
      const ctx = document.getElementById('benchmarkChart').getContext('2d');
      const hasUnified = chartData.SoAUnifiedData !== undefined;
      let datasets = [
        {
          label: 'AoS',
          data: chartData.noSoAData,
          borderColor: '#FF7300',
          backgroundColor: 'rgba(255, 115, 0, 0.1)',
          borderWidth: 2,
          tension: 0.4,
          pointRadius: 5,
          pointHoverRadius: 7
        },
        {
          label: 'SoA',
          data: chartData.SoAData,
          borderColor: '#0088FE',
          backgroundColor: 'rgba(0, 136, 254, 0.1)',
          borderWidth: 2,
          tension: 0.4,
          pointRadius: 5,
          pointHoverRadius: 7
        }
      ];
      if (hasUnified) {
        datasets.push({
          label: 'SoA unified processing',
          data: chartData.SoAUnifiedData,
          borderColor: '#00C49F',
          backgroundColor: 'rgba(0, 196, 159, 0.1)',
          borderWidth: 2,
          tension: 0.4,
          pointRadius: 5,
          pointHoverRadius: 7
        });
      }

      chart = new Chart(ctx, {
        type: 'line',
        data: {
          labels: chartData.labels,
          datasets: datasets
        },
        options: {
          responsive: true,
          maintainAspectRatio: false,
          scales: {
            x: {
              type: 'logarithmic',
              min: 400000,
              max: 5000000,
              title: {
                display: true,
                text: 'Number of Entities',
                font: {
                  size: 14,
                  weight: 'bold'
                },
                padding: {top: 20, bottom: 0}
              },
              ticks: {
                callback: function(value) {
                  if (value === 500000) return "500K";
                  if (value === 1000000) return "1M";
                  if (value === 2000000) return "2M";
                  if (value === 4000000) return "4M";
                  return "";
                },
                maxRotation: 0,
                source: 'data'
              }
            },
            y: {
              title: {
                display: true,
                text: 'Update Time (ms)',
                font: {
                  size: 14,
                  weight: 'bold'
                }
              },
              beginAtZero: true
            }
          },
          plugins: {
            title: {
              display: false
            },
            tooltip: {
              callbacks: {
                title: function(tooltipItems) {
                  return tooltipItems[0].raw.toFixed(6) + ' ms';
                },
                afterTitle: function(tooltipItems) {
                  const datasetIndex = tooltipItems[0].datasetIndex;
                  const dataIndex = tooltipItems[0].dataIndex;
                  let fps = 0;
                  if (datasetIndex === 0) fps = chartData.noSoAFPS[dataIndex];
                  else if (datasetIndex === 1) fps = chartData.SoAFPS[dataIndex];
                  else if (hasUnified && datasetIndex === 2) fps = chartData.SoAUnifiedFPS[dataIndex];
                  return `${fps} FPS`;
                },
                label: function(tooltipItem) {
                  return tooltipItem.dataset.label;
                },
                footer: function(tooltipItems) {
                  const value = tooltipItems[0].label;
                  return formatNumber(value) + " entities";
                }
              }
            },
            legend: {
              position: 'top'
            }
          }
        },
        plugins: [{
          id: 'fpsLabels',
          afterDatasetsDraw: function(chart) {
            const ctx = chart.ctx;
            const chartData = getChartData();
            const hasUnified = chartData.SoAUnifiedData !== undefined;
            chart.data.datasets.forEach((dataset, datasetIndex) => {
              const meta = chart.getDatasetMeta(datasetIndex);
              if (!meta.hidden) {
                meta.data.forEach((element, index) => {
                  ctx.fillStyle = dataset.borderColor;
                  ctx.font = '11px Arial';
                  ctx.textAlign = 'center';
                  let fps = 0;
                  if (datasetIndex === 0) fps = chartData.noSoAFPS[index];
                  else if (datasetIndex === 1) fps = chartData.SoAFPS[index];
                  else if (hasUnified && datasetIndex === 2) fps = chartData.SoAUnifiedFPS[index];
                  const text = `${fps} FPS`;
                  const position = element.getCenterPoint();
                  const yOffset = -15 - (datasetIndex * 15);
                  ctx.fillText(text, position.x, position.y + yOffset);
                });
              }
            });
          }
        }]
      });
    }

    // Initialize chart with default dataset (single-threaded)
    document.addEventListener('DOMContentLoaded', function() {
      updateChartData();
    });</script>



### shameless self-promotion

- I offer training, mentoring, and consulting services. If you are interested, check out [**romeo.training**](https://romeo.training/), alternatively you can reach out at `mail (at) vittorioromeo (dot) com` or [on Twitter](https://twitter.com/supahvee1234).

- Check out my newly-released game on Steam: [**BubbleByte**](https://store.steampowered.com/app/3499760/BubbleByte/) -- it's only $3.99 and one of those games that you can either play actively as a timewaster or more passively to keep you company in the background while you do something else.

- My book [**"Embracing Modern C++ Safely"** is available from all major resellers](http://emcpps.com/).

   - For more information, read this interview: ["Why 4 Bloomberg engineers wrote another C++ book"](https://www.techatbloomberg.com/blog/why-4-bloomberg-engineers-wrote-another-cplusplus-book/)

- If you enjoy fast-paced open-source arcade games with user-created content, check out [**Open Hexagon**](https://store.steampowered.com/app/1358090/Open_Hexagon/), my VRSFML-powered game [available on Steam](https://store.steampowered.com/app/1358090/Open_Hexagon/) and [on itch.io](https://itch.io/t/1758441/open-hexagon-my-spiritual-successor-to-super-hexagon).

   - Open Hexagon is a community-driven spiritual successor to Terry Cavanagh's critically acclaimed Super Hexagon.
