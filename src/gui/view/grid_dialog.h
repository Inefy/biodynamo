#ifndef GUI_GRID_DIALOG_H_
#define GUI_GRID_DIALOG_H_

#include <TSystem.h>
#include <TROOT.h>
#include <TRootHelpDialog.h>
#include <TGButton.h>
#include <TGFrame.h>
#include <TGIcon.h>
#include <TGLabel.h>
#include <TGCanvas.h>
#include <TGIcon.h>
#include <TGPicture.h>
#include <TGNumberEntry.h>
#include "gui/view/model_creator.h"

namespace gui {

class GridDialog : public TGTransientFrame {
 public:
  GridDialog(const TGWindow *p, const TGWindow *main, UInt_t w, UInt_t h,
                    UInt_t options = kMainFrame | kVerticalFrame);
  virtual ~GridDialog();
  void              CloseWindow();
  Bool_t            ProcessMessage(Long_t msg, Long_t parm1, Long_t parm2);

 private:
  /// Main layouts
  std::unique_ptr<TGVerticalFrame>    fVOverall;
  std::unique_ptr<TGVerticalFrame>    fVNumberCells;
  std::unique_ptr<TGVerticalFrame>    fVNumberCellsInner;
  std::unique_ptr<TGVerticalFrame>    fVDistanceCells;
  std::unique_ptr<TGVerticalFrame>    fVDistanceCellsInner;
  std::unique_ptr<TGHorizontalFrame>  fHSelectionLayout;
  std::unique_ptr<TGHorizontalFrame>  fHButtons;
  std::unique_ptr<TGHorizontalFrame>  fHNumberCellsInnerX;
  std::unique_ptr<TGHorizontalFrame>  fHNumberCellsInnerY;
  std::unique_ptr<TGHorizontalFrame>  fHNumberCellsInnerZ;
  std::unique_ptr<TGHorizontalFrame>  fHDistanceCellsInnerX;
  std::unique_ptr<TGHorizontalFrame>  fHDistanceCellsInnerY;
  std::unique_ptr<TGHorizontalFrame>  fHDistanceCellsInnerZ;

  std::unique_ptr<TGNumberEntry>      fEntryNumberX;
  std::unique_ptr<TGNumberEntry>      fEntryNumberY;
  std::unique_ptr<TGNumberEntry>      fEntryNumberZ;

  std::unique_ptr<TGNumberEntry>      fEntryDistanceX;
  std::unique_ptr<TGNumberEntry>      fEntryDistanceY;
  std::unique_ptr<TGNumberEntry>      fEntryDistanceZ;

  std::unique_ptr<TGLabel>            fTitleNumberCells;
  std::unique_ptr<TGLabel>            fNumberCellsBottom;
  std::unique_ptr<TGLabel>            fTitleDistanceCells;
  std::unique_ptr<TGLabel>            fDistanceCellsBottom;
  std::unique_ptr<TGLabel>            fNumberTitleX;
  std::unique_ptr<TGLabel>            fNumberTitleY;
  std::unique_ptr<TGLabel>            fNumberTitleZ;
  std::unique_ptr<TGLabel>            fDistanceTitleX;
  std::unique_ptr<TGLabel>            fDistanceTitleY;
  std::unique_ptr<TGLabel>            fDistanceTitleZ;

  std::unique_ptr<TGCanvas>           fCanvasNumberCells;
  std::unique_ptr<TGCanvas>           fCanvasDistanceCells;

  std::unique_ptr<TGCompositeFrame>   fNumberCellsCompFrame;
  std::unique_ptr<TGCompositeFrame>   fDistanceCellsCompFrame;

  const TGPicture                   *fNumberCellsPicture;
  const TGPicture                   *fDistanceCellsPicture;
  std::unique_ptr<TGIcon>           fNumberCellsIcon;
  std::unique_ptr<TGIcon>           fDistanceCellsIcon;

  std::unique_ptr<TGButton>          fCreateButton;
  std::unique_ptr<TGButton>          fCancelButton;

  std::unique_ptr<TGLayoutHints>    fL1;
  std::unique_ptr<TGLayoutHints>    fL2;
  std::unique_ptr<TGLayoutHints>    fL3;
  std::unique_ptr<TGLayoutHints>    fL4;
  std::unique_ptr<TGLayoutHints>    fL5;
  std::unique_ptr<TGLayoutHints>    fL6;
  std::unique_ptr<TGLayoutHints>    fLTitle;

  void              OnCreate();
  void              OnCancel();
  void              VerifyNumberEntries();
};

}  // namespace gui

#endif // GUI_GRID_DIALOG_H_
