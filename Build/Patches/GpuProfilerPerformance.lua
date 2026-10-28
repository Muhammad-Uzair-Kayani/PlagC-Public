-- Release profiling must observe the renderer without becoming a meaningful part of it.
-- Sampling one frame in four still yields ~30 GPU samples/sec near 120 FPS while reducing
-- timestamp command injection by 75%. Apply equally to OpenGL and Vulkan for fair comparison.
return function(source)
    source = source:gsub("\r\n", "\n")
    local function replace(old, new)
        local first, last = source:find(old, 1, true)
        assert(first, 'GpuProfiler sampling site changed: ' .. old)
        assert(not source:find(old, last + 1, true), 'Ambiguous GpuProfiler performance patch: ' .. old)
        source = source:sub(1, first - 1) .. new .. source:sub(last + 1)
    end

    replace([[void GpuProfiler::BeginPass(GpuPass pass){
#ifndef PC_OGRE_NULL_ONLY
if(!active||pass==GpuPass::Count)return;auto&p=*active->impl_;if(!p.latest.supported)return;auto&slot=p.slots[p.write];]], [[void GpuProfiler::BeginPass(GpuPass pass){
#ifndef PC_OGRE_NULL_ONLY
if(!active||pass==GpuPass::Count)return;auto&p=*active->impl_;if(!p.latest.supported)return;
// Keep profiling statistically useful without placing timestamp commands in every frame.
if((p.frame&3u)!=0u)return;auto&slot=p.slots[p.write];]])

    replace([[void GpuProfiler::EndPass(GpuPass pass){
#ifndef PC_OGRE_NULL_ONLY
if(!active||pass==GpuPass::Count)return;auto&p=*active->impl_;if(!p.latest.supported)return;auto&slot=p.slots[p.write];]], [[void GpuProfiler::EndPass(GpuPass pass){
#ifndef PC_OGRE_NULL_ONLY
if(!active||pass==GpuPass::Count)return;auto&p=*active->impl_;if(!p.latest.supported)return;
if((p.frame&3u)!=0u)return;auto&slot=p.slots[p.write];]])

    return source
end
