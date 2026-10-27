extends RefCounted

# Host liveness policy, NOT an RTA timing constant. Missing post-draw never
# counts as a present. Poll on SceneTree frames so completion cannot resume a
# level load inside RenderingServer's post-draw callback.
const HOST_TIMEOUT_SECONDS := 5.0

static func wait(tree: SceneTree, post_draw: Signal,
		timeout_seconds := HOST_TIMEOUT_SECONDS) -> bool:
	var observed := [false]
	var record := func(): observed[0] = true
	post_draw.connect(record)
	var deadline := Time.get_ticks_usec() + int(timeout_seconds * 1000000.0)
	while not observed[0] and Time.get_ticks_usec() < deadline:
		await tree.process_frame
	post_draw.disconnect(record)
	return observed[0]
