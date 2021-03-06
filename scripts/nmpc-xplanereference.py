import os
import math
import socket
import sys
import time
import ctypes
import vectors
import bisect

import nmpc
nmpc.init()
from nmpc import _cnmpc, state

def values_to_dict(fields, values):
    out = {}
    for i, field in enumerate(fields):
        if field not in out:
            out[field] = values[i]

    return out


def convert_metric(fields, values):
    metric_values = []
    for field, value in zip(fields, values):
        unit = field.rpartition(",")[2].strip("_")
        if unit == "ktas":
            metric_values.append(float(value) * 1.852 / 3.6)
        elif unit == "ftmsl":
            metric_values.append(float(value) * 0.3048)
        elif unit == "lb":
            metric_values.append(float(value) * 0.45359237 * 9.80665)
        elif unit == "ftlb":
            metric_values.append(float(value) * 1.3558179483314004)
        elif unit == "deg":
            metric_values.append(math.radians(float(value)))
        else:
            metric_values.append(float(value))

    return metric_values


def euler_to_q(yaw, pitch, roll):
    return (vectors.Q.rotate("X", -roll) *
            vectors.Q.rotate("Y", -pitch) *
            vectors.Q.rotate("Z", -yaw))


def interpolate_reference(sample_time, points):
    index = bisect.bisect([p[0] for p in points], sample_time)
    if index >= len(points):
        return points[-1]
    delta = [b - a for a, b in zip(points[index-1], points[index])]
    residual_time = ((sample_time - points[index-1][0]) / delta[0])
    new_point = [(a + b*residual_time) \
        for a, b in zip(points[index-1], delta)]

    q = vectors.Q(new_point[7], new_point[8], new_point[9], new_point[10])
    q_norm = q.normalize()
    new_point[7] = q_norm[0]
    new_point[8] = q_norm[1]
    new_point[9] = q_norm[2]
    new_point[10] = q_norm[3]

    return new_point

# Load the data
headers = None
initialised = False
initial_time = 0.0

nmpc.setup(
    state_weights=[1e-1, 1e-1, 1e-1, 1, 1, 1, 1e1, 1e1, 1e1, 1e1, 1e1, 1e1],
    control_weights=[1e-10, 1e-2, 1e-2],
    terminal_weights=[1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1],
    upper_control_bound=[25000, 1.0, 1.0],
    lower_control_bound=[0, -1.0, -1.0])
nmpc.initialise_horizon()

xplane_reference_points = []

fields = ["time",
    "pos_x", "pos_y", "pos_z"
    "vel_n", "vel_e", "vel_d",
    "att_x", "att_y", "att_z", "att_w",
    "angvel_p", "angvel_q", "angvel_r"]
# Load all the X-Plane data in one go
for line in sys.stdin:
    if line.strip() == "":
        continue
    fields = list(field.strip("\n ") \
        for field in line.split("|") if field.strip("\n "))
    if fields[0] == "_real,_time":
        headers = fields
    else:
        data = values_to_dict(headers, fields)
        if not initialised:
            initial_time = float(data["_real,_time"])
            position_offset = [
                float(data["____X,____m"]),
                float(data["____Y,____m"]),
                float(data["____Z,____m"])]
            initialised = True
        attitude = euler_to_q(
            math.radians(float(data["hding,_true"])),
            math.radians(float(data["pitch,__deg"])),
            math.radians(float(data["_roll,__deg"])))
        velocity = (
            -float(data["___vZ,__m/s"]),
            float(data["___vX,__m/s"]),
            -float(data["___vY,__m/s"]))
        out = [
            float(data["_real,_time"]) - initial_time,
            -(float(data["____Z,____m"]) - position_offset[2]),
            (float(data["____X,____m"]) - position_offset[0]),
            -(float(data["____Y,____m"]) - position_offset[1]),
            velocity[0],
            velocity[1],
            velocity[2],
            attitude[0],
            attitude[1],
            attitude[2],
            attitude[3],
            float(data["____P,rad/s"]),
            float(data["____Q,rad/s"]),
            float(data["____R,rad/s"])]

        #print "\t".join(str(o) for o in out)

        xplane_reference_points.append(map(float, out))

# Set up the NMPC reference trajectory using correct interpolation.
for i in xrange(0, nmpc.HORIZON_LENGTH):
    horizon_point = [a for a in interpolate_reference(
        i*nmpc.STEP_LENGTH, xplane_reference_points)]
    horizon_point.extend([15000, 0, 0])
    nmpc.set_reference(horizon_point[1:], i)

# Set up terminal reference. No need for control values as they'd be ignored.
terminal_point = [a for a in interpolate_reference(
    nmpc.HORIZON_LENGTH*nmpc.STEP_LENGTH, xplane_reference_points)]
nmpc.set_reference(terminal_point[1:], nmpc.HORIZON_LENGTH)

initial_point = interpolate_reference(0, xplane_reference_points)
_cnmpc.nmpc_fixedwingdynamics_set_position(*initial_point[1:4])
_cnmpc.nmpc_fixedwingdynamics_set_velocity(*initial_point[4:7])
_cnmpc.nmpc_fixedwingdynamics_set_attitude(
    initial_point[10],
    initial_point[7],
    initial_point[8],
    initial_point[9])
_cnmpc.nmpc_fixedwingdynamics_set_angular_velocity(*initial_point[11:14])
_cnmpc.nmpc_fixedwingdynamics_get_state(state)

for i in xrange(500):
    nmpc.prepare()
    nmpc.solve(
        list(state.position) +
        list(state.velocity) +
        list(state.attitude) +
        list(state.angular_velocity))
    print "%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f" \
        % (i*nmpc.STEP_LENGTH,
            state.position[0],
            state.position[1],
            state.position[2],
            state.attitude[0],
            state.attitude[1],
            state.attitude[2],
            state.attitude[3])
    control_vec = nmpc.get_controls()
    nmpc.integrate(nmpc.STEP_LENGTH, (ctypes.c_double * 3)(*control_vec))
    horizon_point = [a for a in interpolate_reference(
        (i+nmpc.HORIZON_LENGTH)*nmpc.STEP_LENGTH, xplane_reference_points)]
    horizon_point.extend([15000, 0, 0])
    nmpc.update_horizon(horizon_point[1:])
