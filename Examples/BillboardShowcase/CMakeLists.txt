add_freya_example(BillboardShowcase SOURCES Main.cpp IBL none)

# Copy shared font (from GameUiDemo) for in-world nameplates
file(COPY
    "${CMAKE_SOURCE_DIR}/Examples/GameUiDemo/Resources/Fonts"
    DESTINATION
    "${CMAKE_BINARY_DIR}/Examples/BillboardShowcase/Resources")
