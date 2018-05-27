#include <gtk/gtk.h>
#include "phatprivate.h"
#include "gui/phat/phatfanslider.h"
#include "gui/phat/phatvfanslider.h"

static PhatFanSliderClass* parent_class;

static void phat_vfan_slider_class_init (PhatVFanSliderClass* klass);
static void phat_vfan_slider_init (PhatVFanSlider* slider);
#if GTK_CHECK_VERSION(3,0,0)
static void phat_vfan_slider_destroy (GtkWidget* object);
#else
static void phat_vfan_slider_destroy (GtkObject* object);
#endif

GType
phat_vfan_slider_get_type ( )
{
    static GType type = 0;

    if (!type)
    {
        static const GTypeInfo info ={
            sizeof (PhatVFanSliderClass),
            NULL,
            NULL,
            (GClassInitFunc) phat_vfan_slider_class_init,
            NULL,
            NULL,
            sizeof (PhatVFanSlider),
            0,
            (GInstanceInitFunc) phat_vfan_slider_init,
        };

        type = g_type_register_static (PHAT_TYPE_FAN_SLIDER,
                                       "PhatVFanSlider",
                                       &info,
                                       0);
    }

    return type;
}

/**
 * phat_vfan_slider_new:
 * @adjustment: the #GtkAdjustment that the new slider will use
 *
 * Creates a new #PhatVFanSlider.
 *
 * Returns: a newly created #PhatVFanSlider
 * 
 */
GtkWidget*
phat_vfan_slider_new (GtkAdjustment* adjustment)
{
    PhatVFanSlider* slider;

    g_assert (gtk_adjustment_get_lower(adjustment) < gtk_adjustment_get_upper(adjustment));
    g_assert ((gtk_adjustment_get_value(adjustment) >= gtk_adjustment_get_lower(adjustment))
              && (gtk_adjustment_get_value(adjustment) <= gtk_adjustment_get_upper(adjustment)));

    slider = g_object_new (PHAT_TYPE_VFAN_SLIDER, NULL);

    PHAT_FAN_SLIDER (slider)->orientation = GTK_ORIENTATION_VERTICAL;

    phat_fan_slider_set_adjustment (PHAT_FAN_SLIDER (slider), adjustment);

    return (GtkWidget*) slider;
}

/**
 * phat_vfan_slider_new_with_range:
 * @value: the initial value the new slider should have
 * @lower: the lowest value the new slider will allow
 * @upper: the highest value the new slider will allow
 * @step: increment added or subtracted when sliding
 *
 * Creates a new #PhatVFanSlider.  The slider will create a new #GtkAdjustment
 * from @value, @lower, and @upper.  If these parameters represent a bogus
 * configuration, the program will terminate.
 *
 * Returns: a newly created #PhatVFanSlider
 *
 */
GtkWidget*
phat_vfan_slider_new_with_range (double value, double lower,
                                 double upper, double step)
{
    GtkAdjustment* adj;

    adj = (GtkAdjustment*) gtk_adjustment_new (value, lower, upper, step, step, 0);

    return phat_vfan_slider_new (adj);
}

static void
phat_vfan_slider_class_init (PhatVFanSliderClass* klass)
{
#if GTK_CHECK_VERSION(3,0,0)
    GtkWidgetClass* object_class = (GtkWidgetClass*) klass;
#else
    GtkObjectClass* object_class = (GtkObjectClass*) klass;
#endif

//    parent_class = g_type_class_peek (PHAT_TYPE_FAN_SLIDER);
    parent_class = g_type_class_ref (PHAT_TYPE_FAN_SLIDER);

    object_class->destroy = phat_vfan_slider_destroy;
}

static void
phat_vfan_slider_init (PhatVFanSlider* slider)
{
    return;
}

static void
#if GTK_CHECK_VERSION(3,0,0)
phat_vfan_slider_destroy (GtkWidget* object)
#else
phat_vfan_slider_destroy (GtkObject* object)
#endif
{
#if GTK_CHECK_VERSION(3,0,0)
    GtkWidgetClass* klass;
#else
    GtkObjectClass* klass;
#endif

    g_return_if_fail (object != NULL);
    g_return_if_fail (PHAT_IS_VFAN_SLIDER (object));
#if GTK_CHECK_VERSION(3,0,0)
    klass = GTK_WIDGET_CLASS (parent_class);
#else
    klass = GTK_OBJECT_CLASS (parent_class);
#endif

    if (klass->destroy)
        klass->destroy (object);
}
