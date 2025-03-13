cmake_minimum_required (VERSION 3.12)

project ("clither-doc"
    LANGUAGES)

find_package (LATEX COMPONENTS PDFLATEX)
if (NOT LATEX_FOUND)
    message (WARNING "LaTeX was not found. Clither doc will not be generated.")
    return ()
endif ()

if (NOT TARGET clither)
    set (CLITHER_BUILD_DOCDIR ${PROJECT_BINARY_DIR})
    set (CLITHER_INSTALL_DOCDIR "share/doc/clither")
endif ()

# We need to keep track of any changes to .tex files
file (GLOB_RECURSE TEX_FILES CONFIGURE_DEPENDS
    "*.tex")

# The name of the main tex without extension
set (MAIN_TEX "clither-doc")
set (INTERMEDIATE_PDF "${PROJECT_BINARY_DIR}/${MAIN_TEX}.pdf")
set (FINAL_PDF "${CLITHER_BUILD_DOCDIR}/${MAIN_TEX}.pdf")

# Two-pass compilation, then move the final PDF into the build directory.
# Texlive does not have the -aux-directory option so we have to compile
# the doc in a different location and then move the PDF afterwards to not
# clutter up the final directory
add_custom_command (OUTPUT ${FINAL_PDF}
    COMMAND ${PDFLATEX_COMPILER} -output-directory ${PROJECT_BINARY_DIR} -draftmode -interaction=nonstopmode ${MAIN_TEX}.tex
    COMMAND ${PDFLATEX_COMPILER} -output-directory ${PROJECT_BINARY_DIR} -interaction=nonstopmode ${MAIN_TEX}.tex
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CLITHER_BUILD_DOCDIR}
    COMMAND ${CMAKE_COMMAND} -E rename ${INTERMEDIATE_PDF} ${FINAL_PDF}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    DEPENDS ${TEX_FILES}
    COMMENT "Compiling clither doc"
    VERBATIM)
add_custom_target (clither-doc ALL
    DEPENDS ${FINAL_PDF})

if (TARGET clither)
    add_dependencies (clither clither-doc)
endif ()

install (
    FILES ${FINAL_PDF}
    DESTINATION ${CLITHER_INSTALL_DOCDIR})
