#pragma once

namespace aiws::gui {

/**
 * @brief Defines the common reset operation for major M3 GUI components.
 *
 * A reset restores a component to the state it has when the application first
 * starts. Classes that implement this interface provide their own component-
 * specific reset behavior.
 */
class Resettable {
public:
    virtual ~Resettable() = default;

    /**
     * @brief Restores the component to its initial application state.
     */
    virtual void reset() = 0;
};

}  // namespace aiws::gui
