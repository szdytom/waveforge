#include "wforge/fallsand.h"
#include "wforge/xoroshiro.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <iterator>
#include <limits>
#include <map>
#include <queue>
#include <utility>
#include <vector>

#ifndef NDEBUG
#include <cpptrace/cpptrace.hpp>
#include <iostream>
#endif

namespace wf {

namespace {

using Coord = std::array<int, 2>;

template<typename T>
using FrameVector = std::vector<T>;

template<typename K, typename V>
using FrameMap = std::map<K, V>;

template<typename T>
using FrameQueue = std::queue<T>;

// For flow network
struct Edge {
	int y;
	int rev_index;
	int capacity = 0, flow = 0;
	FrameVector<Coord> y_surface;
};

struct CachedPixel {
	PixelType type : 8;
	unsigned int color_index : 8;
	bool ignited : 1;
	PixelElement element;
};

struct Vertex {
	int id, belonged_component = -1;
	int dep, cur_edge; // for Dinic
	int indeg = 0;
	PixelType type;
	FrameVector<Edge> edges;
	FrameVector<Coord> air_surface;
	FrameVector<CachedPixel> cache;
};

struct ConnectedComponent {
	FrameVector<int> vertices;
};

struct AnalysisContext {
	FrameVector<Vertex> vertices;
	FrameMap<std::pair<int, int>, int> edge_idx_map;
	FrameVector<int> pixel_vid;
	FrameVector<ConnectedComponent> components;

	AnalysisContext(int width, int height) noexcept
		: pixel_vid(width * height, -1) {}

	int touchEdge(int u, int v) {
		auto it = edge_idx_map.find({u, v});
		if (it != edge_idx_map.end()) {
			return it->second;
		}

		int ue_id = vertices[u].edges.size();
		int ve_id = vertices[v].edges.size();
		vertices[u].edges.push_back({
			.y = v,
			.rev_index = ve_id,
		});

		vertices[v].edges.push_back({
			.y = u,
			.rev_index = ue_id,
		});

		edge_idx_map[{u, v}] = ue_id;
		edge_idx_map[{v, u}] = ve_id;
		return ue_id;
	}

	// u must be above v
	void incFlow(int u, int v, int x, int y) {
		auto it = edge_idx_map.find({u, v});
		if (it == edge_idx_map.end()) {
#ifndef NDEBUG
			std::cerr << "No edge from " << u << " to " << v << "\n";
			cpptrace::generate_trace().print(std::cerr);
			std::abort();
#endif
			std::unreachable();
		}

		auto &e = vertices[u].edges[it->second];
		e.capacity += 1;
		e.y_surface.push_back(Coord{x, y + 1});

		auto &re = vertices[v].edges[e.rev_index];
		re.capacity += 1;
		re.y_surface.push_back({x, y});
	}
};

constexpr float surface_adjust_factor = 0.7;

constexpr int source_vid = 0;
constexpr int sink_vid = 1;

void densityAnalysisStep(PixelWorld &world) noexcept {
	const int effective_infinity_of_x = world.width() + 10;

	for (int y = world.height() - 2; y >= 0; --y) {
		std::vector<std::pair<int, int>> active_intervals; // [l, r]

		for (int l = 0, r = 0; r < world.width(); ++r) {
			auto tag = world.tagOf(r, y);

			if (tag.pclass == PixelClass::Fluid && r + 1 < world.width()) {
				continue;
			}

			if (r > l) {
				active_intervals.emplace_back(
					l, tag.pclass == PixelClass::Fluid ? r : r - 1
				);
			}
			l = r + 1;
		}

		for (auto [l, r] : active_intervals) {
			if (l == r) {
				continue;
			}

			std::vector<int> fill_pos;
			PixelType fill_type = PixelType::Air;
			for (int x = l; x <= r; ++x) {
				auto tag = world.tagOf(x, y + 1);
				if (tag.pclass != PixelClass::Fluid) {
					continue;
				}

				if (fill_type == PixelType::Air
				    || isDenser(fill_type, tag.type)) {
					fill_type = tag.type;
					fill_pos.clear();
				}

				if (tag.type == fill_type) {
					fill_pos.push_back(x);
				}
			}

			int avail_count = fill_pos.size();
			if (avail_count == 0) {
				continue;
			}

			std::vector<int> left_pos;
			int sp = 0;
			for (int x = l; x <= r; ++x) {
				while (sp < fill_pos.size()
				       && (fill_pos[sp] == -1 || fill_pos[sp] < x)) {
					sp += 1;
				}

				if (sp < fill_pos.size() && fill_pos[sp] == x) {
					left_pos.push_back(x);
				}

				if (isDenserOrEqual(fill_type, world.tagOf(x, y).type)) {
					continue;
				}

				bool has_option = false;
				int left_dis = effective_infinity_of_x;
				int right_dis = effective_infinity_of_x;
				int lp, rp;
				if (!left_pos.empty()) {
					has_option = true;
					lp = left_pos.back();
					left_dis = x - lp;
				}

				if (sp < fill_pos.size()) {
					has_option = true;
					rp = fill_pos[sp];
					right_dis = rp - x;
				}

				if (!has_option) {
					continue;
				}

				avail_count -= 1;
				if (left_dis < right_dis) {
					world.swapFluids(x, y, lp, y + 1);
					left_pos.pop_back();
				} else {
					world.swapFluids(x, y, rp, y + 1);
					fill_pos[sp] = -1;
				}

				if (avail_count == 0) {
					break;
				}
			}
		}
	}
}

void searchConnected(
	PixelWorld &world, AnalysisContext &ctx, int vid, int sx, int sy,
	PixelType ptype
) noexcept {
	constexpr int dx[] = {-1, 1, 0, 0};
	constexpr int dy[] = {0, 0, -1, 1};

	auto &vtx = ctx.vertices[vid];
	int width = world.width();

	FrameVector<Coord> stack;
	stack.push_back({sx, sy});
	while (!stack.empty()) {
		auto [x, y] = stack.back();
		stack.pop_back();

		auto &tag = world.tagOf(x, y);
		if (tag.dirty) {
			continue;
		}

		tag.dirty = true;
		ctx.pixel_vid[y * width + x] = vid;

		for (int i = 0; i < 4; ++i) {
			int nx = x + dx[i];
			int ny = y + dy[i];
			if (!world.inBounds(nx, ny)) {
				continue;
			}

			auto ntag = world.tagOf(nx, ny);
			if (ntag.type != ptype || ntag.dirty) {
				continue;
			}

			stack.push_back({nx, ny});
		}
	}
}

void buildNetwork(PixelWorld &world, AnalysisContext &ctx) noexcept {
	// Reserve 2 vertices for source and sink
	ctx.vertices.push_back({
		.id = 0,
		.type = PixelType::Air,
	});

	ctx.vertices.push_back({
		.id = 1,
		.type = PixelType::Air,
	});

	for (int y = 0; y < world.height(); ++y) {
		for (int x = 0; x < world.width(); ++x) {
			auto tag = world.tagOf(x, y);
			if (tag.pclass != PixelClass::Fluid || tag.dirty) {
				continue;
			}

			int vid = ctx.vertices.size();
			ctx.vertices.push_back({
				.id = vid,
				.type = tag.type,
			});

			searchConnected(world, ctx, vid, x, y, tag.type);
		}
	}

	for (int y = 0; y < world.height() - 1; ++y) {
		for (int x = 0; x < world.width(); ++x) {
			int u = ctx.pixel_vid[y * world.width() + x];
			int v = ctx.pixel_vid[(y + 1) * world.width() + x];
			if (u == -1 || v == -1 || u == v) {
				continue;
			}

			ctx.touchEdge(u, v);
			ctx.incFlow(u, v, x, y);
		}
	}

	// Compute air surfaces
	for (int y = 0; y < world.height(); ++y) {
		for (int x = 0; x < world.width(); ++x) {
			int vid = ctx.pixel_vid[y * world.width() + x];
			if (vid == -1) {
				continue;
			}

			if (y == 0 || world.typeOfIs(x, y - 1, PixelType::Air)) {
				ctx.vertices[vid].air_surface.push_back({x, y});
			}
		}
	}
}

void calculateGraphConnectedComponents(AnalysisContext &ctx) noexcept {
	for (auto &start_v : ctx.vertices) {
		if (start_v.id == source_vid || start_v.id == sink_vid) {
			continue;
		}

		if (start_v.belonged_component != -1) {
			continue;
		}

		int cid = ctx.components.size();
		ctx.components.push_back({});
		FrameVector<int> stack;
		stack.push_back(start_v.id);
		while (!stack.empty()) {
			int u = stack.back();
			stack.pop_back();

			auto &v = ctx.vertices[u];
			if (v.belonged_component != -1) {
				continue;
			}

			v.belonged_component = cid;
			ctx.components[cid].vertices.push_back(u);

			for (auto &e : v.edges) {
				auto &to_v = ctx.vertices[e.y];
				if (to_v.belonged_component == -1) {
					stack.push_back(e.y);
				}
			}
		}
	}
}

bool prepareFlowNetworkOfComponent(
	const PixelWorld &world, AnalysisContext &ctx, int cid
) noexcept {
	int width = world.width(), height = world.height();
	FrameVector<Coord> merged_air_surface;
	for (int vid : ctx.components[cid].vertices) {
		auto &v = ctx.vertices[vid];
		merged_air_surface.insert(
			merged_air_surface.end(), v.air_surface.begin(), v.air_surface.end()
		);
	}

	if (merged_air_surface.size() < 2) {
		return false;
	}

	// sort y from top to bottom (small to large), x undefined
	std::sort(
		merged_air_surface.begin(), merged_air_surface.end(),
		[](const Coord &a, const Coord &b) {
		return a[1] < b[1];
	}
	);

	// shuffle x within same y to avoid bias
	auto &rng = Xoroshiro128PP::globalInstance();
	for (auto it = merged_air_surface.begin(), jt = it;
	     it != merged_air_surface.end(); ++it) {
		if ((*jt)[1] != (*it)[1]) {
			std::shuffle(jt, it, rng);
			jt = it;
		} else if (std::next(it) == merged_air_surface.end()) {
			std::shuffle(jt, merged_air_surface.end(), rng);
		}
	}

	int source_cnt = 1;
	int high_y = merged_air_surface[0][1];
	int low_y = merged_air_surface.back()[1];
	if (low_y <= high_y + 1) {
		return false;
	}

	int n = merged_air_surface.size();

	while (source_cnt < n && merged_air_surface[source_cnt][1] == high_y) {
		source_cnt += 1;
	}

	source_cnt = std::min(source_cnt, n - source_cnt);

	source_cnt = std::min<int>(
		source_cnt, (low_y - high_y) * surface_adjust_factor
	);

	while (source_cnt > 0
	       && merged_air_surface[n - source_cnt][1] == high_y + 1) {
		source_cnt -= 1;
	}

	if (source_cnt == 0) {
		return false;
	}

	// Connect source
	for (int i = 0; i < source_cnt; ++i) {
		auto [sx, sy] = merged_air_surface[i];
		auto v = ctx.pixel_vid[sy * width + sx];

		// not using incFlow(), since we need one side connected to source
		int eid = ctx.touchEdge(source_vid, v);
		auto &e = ctx.vertices[source_vid].edges[eid];
		e.capacity += 1;
		e.y_surface.push_back({sx, sy});
	}

	// Connect sink
	for (int i = n - source_cnt; i < n; ++i) {
		auto [sx, sy] = merged_air_surface[i];
		auto v = ctx.pixel_vid[sy * width + sx];

		// not using incFlow(), since we need one side connected to sink
		int eid = ctx.touchEdge(v, sink_vid);
		auto &e = ctx.vertices[v].edges[eid];
		e.capacity += 1;
		e.y_surface.push_back({sx, sy - 1});
	}

	ctx.components[cid].vertices.push_back(source_vid);
	ctx.components[cid].vertices.push_back(sink_vid);

	return true;
}

bool dinicBFS(AnalysisContext &ctx, int cid) noexcept {
	FrameQueue<int> q;
	for (int vid : ctx.components[cid].vertices) {
		ctx.vertices[vid].dep = 0;
		ctx.vertices[vid].cur_edge = 0;
	}

	ctx.vertices[source_vid].dep = 1;
	q.push(source_vid);
	while (!q.empty()) {
		int u = q.front();
		q.pop();

		for (auto &e : ctx.vertices[u].edges) {
			auto &to_v = ctx.vertices[e.y];
			if (to_v.dep == 0 && e.flow < e.capacity) {
				to_v.dep = ctx.vertices[u].dep + 1;
				q.push(e.y);
			}
		}
	}

	return ctx.vertices[sink_vid].dep != 0;
}

// Maybe use emulated stack to do recursive DFS?
// For now, keep it simple, do real recursion
int dinicDFS(AnalysisContext &ctx, int u, int flow) noexcept {
	if (u == sink_vid) {
		return flow;
	}

	auto &v = ctx.vertices[u];
	int ret = 0;
	for (; v.cur_edge < v.edges.size(); ++v.cur_edge) {
		auto &e = v.edges[v.cur_edge];
		auto &to_v = ctx.vertices[e.y];
		if (to_v.dep == v.dep + 1 && e.flow < e.capacity) {
			int curr_flow = dinicDFS(
				ctx, e.y, std::min(flow - ret, e.capacity - e.flow)
			);
			ret += curr_flow;
			e.flow += curr_flow;
			ctx.vertices[e.y].edges[e.rev_index].flow -= curr_flow;
			if (ret == flow) {
				v.dep = 0;
				return ret;
			}
		}
	}
	return ret;
}

int maxFlow(const PixelWorld &world, AnalysisContext &ctx, int cid) noexcept {
	int maxflow = 0;
	while (dinicBFS(ctx, cid)) {
		maxflow += dinicDFS(ctx, source_vid, std::numeric_limits<int>::max());
	}
	return maxflow;
}

void applyFlowResults(
	PixelWorld &world, AnalysisContext &ctx, int cid
) noexcept {
	for (int vid : ctx.components[cid].vertices) {
		auto &v = ctx.vertices[vid];
		for (auto &e : v.edges) {
			if (e.flow > 0) {
				ctx.vertices[e.y].indeg += 1;
			}
		}
	}

	// Topsort
	FrameQueue<int> q;
	q.push(source_vid);
	auto rng = Xoroshiro128PP::globalInstance();
	while (!q.empty()) {
		int u = q.front();
		q.pop();

		auto &v = ctx.vertices[u];
		if (u != source_vid) {
			std::shuffle(v.cache.begin(), v.cache.end(), rng);
		}

		for (auto &e : v.edges) {
			auto &to_v = ctx.vertices[e.y];
			if (e.flow <= 0) {
				continue;
			}

			// TODO: partial shuffle (Knuth shuffle)
			std::shuffle(e.y_surface.begin(), e.y_surface.end(), rng);
			for (int f = 0; f < e.flow; ++f) {
				auto [x, y] = e.y_surface[f];
				auto &tag = world.tagOf(x, y);
				to_v.cache.push_back({
					.type = tag.type,
					.color_index = tag.color_index,
					.ignited = tag.ignited,
					.element = std::move(world.elementOf(x, y)),
				});

				if (u == source_vid) {
					world.replacePixelWithAir(x, y);
				} else {
					auto &cp = v.cache.back();
					tag.pclass = PixelClass::Fluid;
					tag.type = cp.type;
					tag.color_index = cp.color_index;
					tag.ignited = cp.ignited;
					world.elementOf(x, y) = std::move(cp.element);
					v.cache.pop_back();
				}
			}

			to_v.indeg -= 1;
			e.flow = 0;
			if (e.y != sink_vid && to_v.indeg == 0) {
				q.push(e.y);
			}
		}
	}
}

void analysisFlow(PixelWorld &world, AnalysisContext &ctx) noexcept {
	for (int i = 0; i < ctx.components.size(); ++i) {
		if (prepareFlowNetworkOfComponent(world, ctx, i)) {
			int flow = maxFlow(world, ctx, i);
			applyFlowResults(world, ctx, i);
		}

		// Reset S, T connections
		ctx.vertices[source_vid].edges.clear();
		ctx.vertices[sink_vid].edges.clear();
		ctx.vertices[source_vid].indeg = 0;
		ctx.vertices[sink_vid].indeg = 0;
	}
}

} // namespace

void PixelWorld::fluidAnalysisStep() noexcept {
	AnalysisContext ctx(_width, _height);
	densityAnalysisStep(*this);

	buildNetwork(*this, ctx);
	resetDirtyFlags();

	calculateGraphConnectedComponents(ctx);
	analysisFlow(*this, ctx);
}

} // namespace wf
