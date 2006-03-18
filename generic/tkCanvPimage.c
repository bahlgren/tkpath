/*
 * tkCanvPimage.c --
 *
 *	This file implements an image canvas item modelled after its
 *  SVG counterpart. See http://www.w3.org/TR/SVG11/.
 *
 * Copyright (c) 2006  Mats Bengtsson
 *
 * $Id$
 */

#include "tkIntPath.h"
#include "tkCanvPathUtil.h"
#include "tkPathCopyTk.h"

/* For debugging. */
extern Tcl_Interp *gInterp;


/*
 * The structure below defines the record for each path item.
 */

typedef struct PimageItem  {
    Tk_Item header;			/* Generic stuff that's the same for all
                             * types.  MUST BE FIRST IN STRUCTURE. */
    Tk_Canvas canvas;		/* Canvas containing item. */
    Tk_PathStyle style;		/* Contains most drawing info. */
    char *styleName;		/* Name of any inherited style object. */
    char *imageString;		/* String describing -image option (malloc-ed).
                             * NULL means no image right now. */
    Tk_Image image;			/* Image to display in window, or NULL if
                             * no image at present. */
    double width;			/* If 0 use natural width or height. */
    double height;
    PathRect bbox;			/* Bounding box with zero width outline.
                             * Untransformed coordinates. */
    long flags;				/* Various flags. */
    char *null;   			/* Just a placeholder for not yet implemented stuff. */ 
} PimageItem;


/*
 * Prototypes for procedures defined in this file:
 */

static void		ComputePimageBbox(Tk_Canvas canvas, PimageItem *pimagePtr);
static int		ConfigurePimage(Tcl_Interp *interp, Tk_Canvas canvas, 
                        Tk_Item *itemPtr, int objc,
                        Tcl_Obj *CONST objv[], int flags);
static int		CreatePimage(Tcl_Interp *interp,
                        Tk_Canvas canvas, struct Tk_Item *itemPtr,
                        int objc, Tcl_Obj *CONST objv[]);
static void		DeletePimage(Tk_Canvas canvas,
                        Tk_Item *itemPtr, Display *display);
static void		DisplayPimage(Tk_Canvas canvas,
                        Tk_Item *itemPtr, Display *display, Drawable drawable,
                        int x, int y, int width, int height);
static int		PimageCoords(Tcl_Interp *interp,
                        Tk_Canvas canvas, Tk_Item *itemPtr,
                        int objc, Tcl_Obj *CONST objv[]);
static int		PimageToArea(Tk_Canvas canvas,
                        Tk_Item *itemPtr, double *rectPtr);
static double	PimageToPoint(Tk_Canvas canvas,
                        Tk_Item *itemPtr, double *coordPtr);
static int		PimageToPostscript(Tcl_Interp *interp,
                        Tk_Canvas canvas, Tk_Item *itemPtr, int prepass);
static void		ScalePimage(Tk_Canvas canvas,
                        Tk_Item *itemPtr, double originX, double originY,
                        double scaleX, double scaleY);
static void		TranslatePimage(Tk_Canvas canvas,
                        Tk_Item *itemPtr, double deltaX, double deltaY);
static void		ImageChangedProc _ANSI_ARGS_((ClientData clientData,
                        int x, int y, int width, int height, int imgWidth,
                        int imgHeight));


PATH_STYLE_CUSTOM_OPTION_RECORDS

static Tk_ConfigSpec configSpecs[] = {
    {TK_CONFIG_DOUBLE, "-height", (char *) NULL, (char *) NULL,
            "0", Tk_Offset(PimageItem, height), TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_STRING, "-image", (char *) NULL, (char *) NULL,
            (char *) NULL, Tk_Offset(PimageItem, imageString), TK_CONFIG_NULL_OK},
    {TK_CONFIG_DOUBLE, "-width", (char *) NULL, (char *) NULL,
            "0", Tk_Offset(PimageItem, width), TK_CONFIG_DONT_SET_DEFAULT},
    PATH_CONFIG_SPEC_STYLE_MATRIX(PimageItem),
    PATH_CONFIG_SPEC_CORE(PimageItem),
    PATH_END_CONFIG_SPEC
};

/*
 * The structures below defines the 'prect' item type by means
 * of procedures that can be invoked by generic item code.
 */

Tk_ItemType tkPimageType = {
    "pimage",						/* name */
    sizeof(PimageItem),				/* itemSize */
    CreatePimage,					/* createProc */
    configSpecs,					/* configSpecs */
    ConfigurePimage,					/* configureProc */
    PimageCoords,					/* coordProc */
    DeletePimage,					/* deleteProc */
    DisplayPimage,					/* displayProc */
    TK_CONFIG_OBJS,					/* flags */
    PimageToPoint,					/* pointProc */
    PimageToArea,					/* areaProc */
    PimageToPostscript,				/* postscriptProc */
    ScalePimage,						/* scaleProc */
    TranslatePimage,					/* translateProc */
    (Tk_ItemIndexProc *) NULL,		/* indexProc */
    (Tk_ItemCursorProc *) NULL,		/* icursorProc */
    (Tk_ItemSelectionProc *) NULL,	/* selectionProc */
    (Tk_ItemInsertProc *) NULL,		/* insertProc */
    (Tk_ItemDCharsProc *) NULL,		/* dTextProc */
    (Tk_ItemType *) NULL,			/* nextPtr */
};
                        
 

static int		
CreatePimage(Tcl_Interp *interp, Tk_Canvas canvas, struct Tk_Item *itemPtr,
        int objc, Tcl_Obj *CONST objv[])
{
    PimageItem *pimagePtr = (PimageItem *) itemPtr;
    int	i;

    if (objc == 0) {
        Tcl_Panic("canvas did not pass any coords\n");
    }
    gInterp = interp;

    /*
     * Carry out initialization that is needed to set defaults and to
     * allow proper cleanup after errors during the the remainder of
     * this procedure.
     */
    Tk_CreatePathStyle(&(pimagePtr->style));
    pimagePtr->canvas = canvas;
    pimagePtr->styleName = NULL;
    pimagePtr->imageString = NULL;
    pimagePtr->image = NULL;
    pimagePtr->height = 0;
    pimagePtr->width = 0;
    pimagePtr->bbox = NewEmptyPathRect();
    pimagePtr->flags = 0L;
    
    for (i = 1; i < objc; i++) {
        char *arg = Tcl_GetString(objv[i]);
        if ((arg[0] == '-') && (arg[1] >= 'a') && (arg[1] <= 'z')) {
            break;
        }
    }    
    if (PimageCoords(interp, canvas, itemPtr, i, objv) != TCL_OK) {
        goto error;
    }
    if (ConfigurePimage(interp, canvas, itemPtr, objc-i, objv+i, 0) == TCL_OK) {
        return TCL_OK;
    }

    error:
    DeletePimage(canvas, itemPtr, Tk_Display(Tk_CanvasTkwin(canvas)));
    return TCL_ERROR;
}

static int		
PimageCoords(Tcl_Interp *interp, Tk_Canvas canvas, Tk_Item *itemPtr, 
        int objc, Tcl_Obj *CONST objv[])
{
    PimageItem *pimagePtr = (PimageItem *) itemPtr;
    int result;



    return result;
}

void
ComputePimageBbox(Tk_Canvas canvas, PimageItem *pimagePtr)
{
    Tk_PathStyle *stylePtr = &(pimagePtr->style);
    Tk_State state = pimagePtr->header.state;

    if(state == TK_STATE_NULL) {
        state = ((TkCanvas *)canvas)->canvas_state;
    }


}

static int		
ConfigurePimage(Tcl_Interp *interp, Tk_Canvas canvas, Tk_Item *itemPtr, 
        int objc, Tcl_Obj *CONST objv[], int flags)
{
    PimageItem *pimagePtr = (PimageItem *) itemPtr;
    Tk_PathStyle *stylePtr = &(pimagePtr->style);
    Tk_Window tkwin;
    Tk_State state;
    unsigned long mask;
    Tk_Image image;

    tkwin = Tk_CanvasTkwin(canvas);
    if (TCL_OK != Tk_ConfigureWidget(interp, tkwin, configSpecs, objc,
            (CONST char **) objv, (char *) pimagePtr, flags|TK_CONFIG_OBJS)) {
        return TCL_ERROR;
    }
    
    /*
     * If we have got a style name it's options take precedence
     * over the actual path configuration options. This is how SVG does it.
     * Good or bad?
     */
    if (pimagePtr->styleName != NULL) {
        PathStyleMergeStyles(tkwin, stylePtr, pimagePtr->styleName, 
                kPathMergeStyleNotFill);
    }     
    /*
     * Create the image.  Save the old image around and don't free it
     * until after the new one is allocated.  This keeps the reference
     * count from going to zero so the image doesn't have to be recreated
     * if it hasn't changed.
     */

    if (pimagePtr->imageString != NULL) {
        image = Tk_GetImage(interp, tkwin, pimagePtr->imageString,
                ImageChangedProc, (ClientData) pimagePtr);
        if (image == NULL) {
            return TCL_ERROR;
        }
    } else {
        image = NULL;
    }
    if (pimagePtr->image != NULL) {
        Tk_FreeImage(pimagePtr->image);
    }
    pimagePtr->image = image;
    
    /*
     * Recompute bounding box for path.
     */
    ComputePimageBbox(canvas, pimagePtr);
    return TCL_OK;
}

static void		
DeletePimage(Tk_Canvas canvas, Tk_Item *itemPtr, Display *display)
{
    PimageItem *pimagePtr = (PimageItem *) itemPtr;

    if (pimagePtr->imageString != NULL) {
        ckfree(pimagePtr->imageString);
    }
    if (pimagePtr->image != NULL) {
        Tk_FreeImage(pimagePtr->image);
    }
}

static void		
DisplayPimage(Tk_Canvas canvas, Tk_Item *itemPtr, Display *display, Drawable drawable,
        int x, int y, int width, int height)
{
    PimageItem *pimagePtr = (PimageItem *) itemPtr;
    TMatrix m = GetCanvasTMatrix(canvas);
    Tk_PathStyle *stylePtr = &(pimagePtr->style);
    TkPathContext ctx;

    ctx = TkPathInit(display, drawable);
    TkPathPushTMatrix(ctx, &m);
    if (stylePtr->matrixPtr != NULL) {
        TkPathPushTMatrix(ctx, stylePtr->matrixPtr);
    }

    //TkPathImage(ctx, XImage *image, double x, double y, pimagePtr->width, pimagePtr->height);
    TkPathFree(ctx);
}

static double	
PimageToPoint(Tk_Canvas canvas, Tk_Item *itemPtr, double *pointPtr)
{
    PimageItem *pimagePtr = (PimageItem *) itemPtr;
    double x1, x2, y1, y2, xDiff, yDiff;

    x1 = pimagePtr->header.x1;
    y1 = pimagePtr->header.y1;
    x2 = pimagePtr->header.x2;
    y2 = pimagePtr->header.y2;

    /*
     * Point is outside rectangle.
     */

    if (pointPtr[0] < x1) {
        xDiff = x1 - pointPtr[0];
    } else if (pointPtr[0] > x2)  {
        xDiff = pointPtr[0] - x2;
    } else {
        xDiff = 0;
    }

    if (pointPtr[1] < y1) {
        yDiff = y1 - pointPtr[1];
    } else if (pointPtr[1] > y2)  {
        yDiff = pointPtr[1] - y2;
    } else {
        yDiff = 0;
    }

    return hypot(xDiff, yDiff);
}

static int		
PimageToArea(Tk_Canvas canvas, Tk_Item *itemPtr, double *areaPtr)
{
    PimageItem *pimagePtr = (PimageItem *) itemPtr;
    
    if ((areaPtr[2] <= pimagePtr->header.x1)
            || (areaPtr[0] >= pimagePtr->header.x2)
            || (areaPtr[3] <= pimagePtr->header.y1)
            || (areaPtr[1] >= pimagePtr->header.y2)) {
        return -1;
    }
    if ((areaPtr[0] <= pimagePtr->header.x1)
            && (areaPtr[1] <= pimagePtr->header.y1)
            && (areaPtr[2] >= pimagePtr->header.x2)
            && (areaPtr[3] >= pimagePtr->header.y2)) {
        return 1;
    }
    return 0;
}

static int		
PimageToPostscript(Tcl_Interp *interp, Tk_Canvas canvas, Tk_Item *itemPtr, int prepass)
{
    return TCL_ERROR;
}

static void		
ScalePimage(Tk_Canvas canvas, Tk_Item *itemPtr, double originX, double originY,
        double scaleX, double scaleY)
{


}

static void		
TranslatePimage(Tk_Canvas canvas, Tk_Item *itemPtr, double deltaX, double deltaY)
{
    PimageItem *pimagePtr = (PimageItem *) itemPtr;

    /* Just translate the bbox'es as well. */
    TranslatePathRect(&(pimagePtr->bbox), deltaX, deltaY);

    pimagePtr->header.x1 = (int) pimagePtr->bbox.x1;
    pimagePtr->header.x2 = (int) pimagePtr->bbox.x2;
    pimagePtr->header.y1 = (int) pimagePtr->bbox.y1;
    pimagePtr->header.y2 = (int) pimagePtr->bbox.y2;
}

static void
ImageChangedProc(clientData, x, y, width, height, imgWidth, imgHeight)
    ClientData clientData;		/* Pointer to canvas item for image. */
    int x, y;					/* Upper left pixel (within image)
                                 * that must be redisplayed. */
    int width, height;			/* Dimensions of area to redisplay
                                 * (may be <= 0). */
    int imgWidth, imgHeight;	/* New dimensions of image. */
{
    PimageItem *pimagePtr = (PimageItem *) clientData;

    /*
     * If the image's size changed and it's not anchored at its
     * northwest corner then just redisplay the entire area of the
     * image.  This is a bit over-conservative, but we need to do
     * something because a size change also means a position change.
     */

    if (((pimagePtr->header.x2 - pimagePtr->header.x1) != imgWidth)
            || ((pimagePtr->header.y2 - pimagePtr->header.y1) != imgHeight)) {
        x = y = 0;
        width = imgWidth;
        height = imgHeight;
        Tk_CanvasEventuallyRedraw(pimagePtr->canvas, pimagePtr->header.x1,
                pimagePtr->header.y1, pimagePtr->header.x2, pimagePtr->header.y2);
    } 
    ComputePimageBbox(pimagePtr->canvas, pimagePtr);
    Tk_CanvasEventuallyRedraw(pimagePtr->canvas, pimagePtr->header.x1 + x,
            pimagePtr->header.y1 + y, (int) (pimagePtr->header.x1 + x + width),
            (int) (pimagePtr->header.y1 + y + height));
}

/*----------------------------------------------------------------------*/
