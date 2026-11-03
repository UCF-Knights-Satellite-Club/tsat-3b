#pragma once
/*
 * Mission.h - where to land and how to get there.
 *
 * Get coordinates from Google Maps (right-click -> the lat, lon pair) and use
 * at least 6 decimal places (0.000001 deg is about 0.1 m).
 *
 * How the path is flown:
 *   release point -> PATH[0] -> PATH[1] -> ... -> PATH[last] -> TARGET
 *
 *   - Lay the waypoints out so every straight leg between them is clear of
 *     trees and buildings, with at least one TURN_RADIUS_M of clear space on
 *     each side of the line.
 *   - The last leg (PATH[last] -> TARGET) is the final approach. Point it into
 *     the expected wind and keep it at least 40-60 m long so the parafoil has
 *     time to line up and burn off extra height with the brakes.
 *   - ZIGZAG_LEG picks one leg to fly back and forth on to burn off extra
 *     height. Because the glide ratio is high, this is where most of the
 *     height is lost. It must be a straight, open corridor. The back legs run
 *     on a parallel line to the ZIGZAG_SIDE_RIGHT side (Config.h), so that
 *     side needs about ZIGZAG_SPACING_M + TURN_RADIUS_M of clear width.
 *     The length of each back-and-forth is worked out in flight from the
 *     measured sink rate and wind, so it burns just the spare height.
 *     Laying the corridor across the expected wind makes both directions cost
 *     about the same and gives the wind estimator good data.
 *
 * The values below are an EXAMPLE field: replace them with your own.
 *
 *            N
 *            ^      PATH[0] (150 N, 60 W) <====zig-zag====> PATH[1] (150 N, 60 E)
 *            |                                                  /
 *            |                        PATH[2] (60 N, 0) <------'
 *            |                           |  final approach (heading south)
 *            |                         TARGET
 */

struct GeoPoint {
  double lat;
  double lon;
};

constexpr GeoPoint TARGET = {34.000000, -117.000000};

constexpr GeoPoint PATH[] = {
  {34.001349, -117.000651},  // PATH[0] start of the zig-zag corridor
  {34.001349, -116.999349},  // PATH[1] end of the zig-zag corridor
  {34.000540, -117.000000},  // PATH[2] start of final approach
};
constexpr int PATH_COUNT = sizeof(PATH) / sizeof(PATH[0]);

// Zig-zag on the leg PATH[ZIGZAG_LEG] -> PATH[ZIGZAG_LEG + 1]. -1 = never zig-zag.
// It cannot be the final approach leg.
constexpr int ZIGZAG_LEG = 0;

// Target ground height minus the ground height where the controller was
// switched on (the altimeter zeroes itself at power-on). 0 if both are the same field.
constexpr float TARGET_ELEVATION_M = 0.0f;

static_assert(PATH_COUNT >= 1, "PATH needs at least one waypoint (the start of final approach)");
static_assert(ZIGZAG_LEG < PATH_COUNT - 1, "ZIGZAG_LEG must be a leg between two PATH points");
