add_freya_example(BillboardShowcase SOURCES Main.cpp IBL none)

# Copy shared font (from GameUiShowcase) for in-world nameplates
file(COPY
    "${CMAKE_SOURCE_DIR}/Examples/GameUiShowcase/Resources/Fonts"
    DESTINATION
    "${CMAKE_BINARY_DIR}/Examples/BillboardShowcase/Resources")
