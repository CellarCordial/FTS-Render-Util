#define THREAD_GROUP_NUM_X 1

ByteAddressBuffer prefix_sum_buffer : register(t0);
RWByteAddressBuffer ircache_entry_occupy_buffer : register(u0);

#if defined(THREAD_GROUP_NUM_X)

#define SHARED_SIZE THREAD_GROUP_NUM_X * 2

groupshared uint32_t shared_buffer[SHARED_SIZE];

[shader("compute")]
[numthreads(THREAD_GROUP_NUM_X, 1, 1)]
void main(uint3 thread_id: SV_DispatchThreadID, uint3 group_id : SV_GroupID)
{
    uint32_t index = thread_id.x;
    uint32_t offset = group_id.x;

    uint32_t process_index = (index * 2 + offset * SHARED_SIZE) * sizeof(uint32_t);

    uint2 sum = ircache_entry_occupy_buffer.Load2(process_index);
    uint2 former_sum = offset == 0 ? 0 : prefix_sum_buffer.Load2((offset - 1) * SHARED_SIZE);

    ircache_entry_occupy_buffer.Store2(process_index, sum + former_sum);
}


#endif