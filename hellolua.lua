function main()
    local v1 = vec_sample.new(100, 200, 300)
    local v2 = vec_sample.new(10, 20, 30)
    print(v1, v2)

    v1.x = -10
    v2.y = -20
    print(v1, v2)

    print(v1.x, v2.y, v1.z)

    local v3 = v1:new()
    print(v1, v3)

    v3.x = 100500

    print(v1, v3)
end


