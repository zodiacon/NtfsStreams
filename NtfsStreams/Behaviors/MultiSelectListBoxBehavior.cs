using NtfsStreams.ViewModels;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Interactivity;

namespace NtfsStreams.Behaviors {
	class MultiSelectListBoxBehavior : Behavior<ListBox> {
		protected override void OnAttached() {
			base.OnAttached();

			AssociatedObject.SelectionChanged += AssociatedObject_SelectionChanged;
		}

		protected override void OnDetaching() {
			AssociatedObject.SelectionChanged -= AssociatedObject_SelectionChanged;

			base.OnDetaching();
		}



		public FileStreamsViewModel[] SelectedItems {
			get { return (FileStreamsViewModel[])GetValue(SelectedItemsProperty); }
			set { SetValue(SelectedItemsProperty, value); }
		}

		public static readonly DependencyProperty SelectedItemsProperty =
			 DependencyProperty.Register(nameof(SelectedItems), typeof(FileStreamsViewModel[]), typeof(MultiSelectListBoxBehavior), new PropertyMetadata(null));


		private void AssociatedObject_SelectionChanged(object sender, SelectionChangedEventArgs e) {
			if (SelectedItems == null || SelectedItems.Length != AssociatedObject.SelectedItems.Count)
				SelectedItems = new FileStreamsViewModel[AssociatedObject.SelectedItems.Count];
			AssociatedObject.SelectedItems.CopyTo(SelectedItems, 0);
		}
	}
}
